#include "editor/project/EngineSdk.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <knownfolders.h>
#include <objbase.h>
#include <shlobj.h>
#include <winhttp.h>

#include <cstdint>
#include <cwchar>
#include <fstream>
#include <system_error>
#include <utility>
#include <vector>

namespace BlackLight::Editor
{

	namespace
	{

		/// Converts UTF-8 text to UTF-16.
		std::wstring Widen(std::string_view text)
		{
			if (text.empty())
				return {};
			const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(),
				static_cast<int>(text.size()), nullptr, 0);
			if (length <= 0)
				return {};
			std::wstring result(static_cast<std::size_t>(length), L'\0');
			MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
				result.data(), length);
			return result;
		}

		/// Converts UTF-16 text to UTF-8.
		std::string Narrow(std::wstring_view text)
		{
			if (text.empty())
				return {};
			const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(),
				static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
			if (length <= 0)
				return {};
			std::string result(static_cast<std::size_t>(length), '\0');
			WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
				result.data(), length, nullptr, nullptr);
			return result;
		}

		/// Returns a readable description of the last Win32 error.
		std::string LastErrorMessage(std::string_view action)
		{
			return std::string(action) + " (error " + std::to_string(GetLastError()) + ")";
		}

		/// Reports a progress line when a handler is installed.
		void Report(const EngineSdk::ProgressHandler &progress, std::string_view message)
		{
			if (progress)
				progress(message);
		}

		/// Owns a WinHTTP handle for the duration of a request.
		class InternetHandle final
		{
		public:
			InternetHandle() = default;
			explicit InternetHandle(HINTERNET handle) : handle_(handle) {}
			~InternetHandle() { Reset(); }

			InternetHandle(const InternetHandle &) = delete;
			InternetHandle &operator=(const InternetHandle &) = delete;

			InternetHandle(InternetHandle &&other) noexcept : handle_(other.handle_)
			{
				other.handle_ = nullptr;
			}

			InternetHandle &operator=(InternetHandle &&other) noexcept
			{
				if (this != &other)
				{
					Reset();
					handle_ = other.handle_;
					other.handle_ = nullptr;
				}
				return *this;
			}

			void Reset() noexcept
			{
				if (handle_)
				{
					WinHttpCloseHandle(handle_);
					handle_ = nullptr;
				}
			}

			operator HINTERNET() const noexcept { return handle_; }
			explicit operator bool() const noexcept { return handle_ != nullptr; }

		private:
			HINTERNET handle_ = nullptr;
		};

		/// Splits an HTTP URL into the components WinHTTP needs.
		struct UrlParts
		{
			std::wstring host;
			std::wstring path;
			INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
			bool secure = true;
		};

		/// Parses an HTTP or HTTPS URL.
		bool CrackUrl(const std::wstring &url, UrlParts &parts, std::string &error)
		{
			URL_COMPONENTS components{};
			components.dwStructSize = sizeof(components);
			components.dwSchemeLength = static_cast<DWORD>(-1);
			components.dwHostNameLength = static_cast<DWORD>(-1);
			components.dwUrlPathLength = static_cast<DWORD>(-1);
			components.dwExtraInfoLength = static_cast<DWORD>(-1);
			if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &components))
			{
				error = LastErrorMessage("Failed to parse the engine download URL");
				return false;
			}
			parts.host.assign(components.lpszHostName, components.dwHostNameLength);
			parts.path.assign(components.lpszUrlPath, components.dwUrlPathLength);
			if (components.dwExtraInfoLength > 0)
				parts.path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
			parts.port = components.nPort;
			parts.secure = components.nScheme == INTERNET_SCHEME_HTTPS;
			return true;
		}

		/// Opens a WinHTTP session with bounded timeouts.
		InternetHandle OpenSession(std::string &error)
		{
			InternetHandle session(WinHttpOpen(L"BlackLightEditor/0.1",
				WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
				WINHTTP_NO_PROXY_BYPASS, 0));
			if (!session)
			{
				error = LastErrorMessage("Failed to open an HTTP session");
				return session;
			}
			WinHttpSetTimeouts(session, 15000, 15000, 30000, 60000);
			return session;
		}

		/// Sends one request and returns the response handle.
		InternetHandle SendRequest(HINTERNET session, const UrlParts &parts,
			const wchar_t *method, bool follow_redirects, std::string &error)
		{
			InternetHandle connection(
				WinHttpConnect(session, parts.host.c_str(), parts.port, 0));
			if (!connection)
			{
				error = LastErrorMessage("Failed to connect to the engine download host");
				return {};
			}

			const DWORD flags = parts.secure ? WINHTTP_FLAG_SECURE : 0;
			InternetHandle request(WinHttpOpenRequest(connection, method,
				parts.path.c_str(), nullptr, WINHTTP_NO_REFERER,
				WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
			if (!request)
			{
				error = LastErrorMessage("Failed to create the engine download request");
				return {};
			}

			DWORD policy = follow_redirects ? WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS
										   : WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
			WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &policy,
				sizeof(policy));

			if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
					WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
				!WinHttpReceiveResponse(request, nullptr))
			{
				error = LastErrorMessage("Failed to reach the engine download host");
				return {};
			}
			return request;
		}

		/// Reads the numeric HTTP status code of a response.
		bool QueryStatusCode(HINTERNET request, DWORD &status, std::string &error)
		{
			DWORD size = sizeof(status);
			if (!WinHttpQueryHeaders(request,
					WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
					WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX))
			{
				error = LastErrorMessage("Failed to read the engine download status");
				return false;
			}
			return true;
		}

		/// Resolves the newest release tag and its download URL from the GitHub redirect.
		bool ResolveLatestRelease(std::string &tag, std::wstring &download_url,
			std::string &error)
		{
			const std::wstring latest_url = L"https://github.com/" +
				Widen(EngineSdk::Repository) + L"/releases/latest/download/" +
				Widen(EngineSdk::AssetName);

			UrlParts parts;
			if (!CrackUrl(latest_url, parts, error))
				return false;

			InternetHandle session = OpenSession(error);
			if (!session)
				return false;
			InternetHandle request = SendRequest(session, parts, L"HEAD", false, error);
			if (!request)
				return false;

			DWORD status = 0;
			if (!QueryStatusCode(request, status, error))
				return false;
			if (status < 300 || status >= 400)
			{
				error = "The latest BlackLight release did not redirect (HTTP " +
					std::to_string(status) + ")";
				return false;
			}

			DWORD length = 0;
			WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX,
				WINHTTP_NO_OUTPUT_BUFFER, &length, WINHTTP_NO_HEADER_INDEX);
			if (length == 0 || GetLastError() != ERROR_INSUFFICIENT_BUFFER)
			{
				error = "The latest BlackLight release has no download location";
				return false;
			}

			std::wstring location(static_cast<std::size_t>(length) / sizeof(wchar_t), L'\0');
			if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION,
					WINHTTP_HEADER_NAME_BY_INDEX, location.data(), &length,
					WINHTTP_NO_HEADER_INDEX))
			{
				error = LastErrorMessage("Failed to read the engine download location");
				return false;
			}
			location.resize(wcslen(location.c_str()));

			const std::wstring marker = L"/releases/download/";
			const std::size_t marker_position = location.find(marker);
			if (marker_position == std::wstring::npos)
			{
				error = "The latest BlackLight release location is not a release download";
				return false;
			}
			const std::size_t tag_begin = marker_position + marker.size();
			const std::size_t tag_end = location.find(L'/', tag_begin);
			if (tag_end == std::wstring::npos || tag_end == tag_begin)
			{
				error = "The latest BlackLight release tag could not be determined";
				return false;
			}

			tag = Narrow(location.substr(tag_begin, tag_end - tag_begin));
			download_url = std::move(location);
			return true;
		}

		/// Streams a URL into a local file while reporting percentage progress.
		bool DownloadFile(const std::wstring &url, const std::filesystem::path &destination,
			std::string &error, const EngineSdk::ProgressHandler &progress,
			const EngineSdk::CancelCheck &cancel)
		{
			UrlParts parts;
			if (!CrackUrl(url, parts, error))
				return false;

			InternetHandle session = OpenSession(error);
			if (!session)
				return false;
			InternetHandle request = SendRequest(session, parts, L"GET", true, error);
			if (!request)
				return false;

			DWORD status = 0;
			if (!QueryStatusCode(request, status, error))
				return false;
			if (status != 200)
			{
				error = "The engine download failed with HTTP " + std::to_string(status);
				return false;
			}

			DWORD total = 0;
			DWORD size = sizeof(total);
			WinHttpQueryHeaders(request,
				WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
				WINHTTP_HEADER_NAME_BY_INDEX, &total, &size, WINHTTP_NO_HEADER_INDEX);

			std::ofstream output(destination, std::ios::binary | std::ios::trunc);
			if (!output)
			{
				error = "Failed to open " + destination.u8string() + " for writing";
				return false;
			}

			std::vector<char> buffer(64 * 1024);
			std::uint64_t written = 0;
			int last_percent = -1;
			for (;;)
			{
				if (cancel && cancel())
				{
					error = "The engine download was cancelled";
					return false;
				}

				DWORD available = 0;
				if (!WinHttpQueryDataAvailable(request, &available))
				{
					error = LastErrorMessage("Reading the engine download failed");
					return false;
				}
				if (available == 0)
					break;

				DWORD read = 0;
				if (!WinHttpReadData(request, buffer.data(),
						static_cast<DWORD>(buffer.size()), &read))
				{
					error = LastErrorMessage("Reading the engine download failed");
					return false;
				}
				if (read == 0)
					break;

				output.write(buffer.data(), static_cast<std::streamsize>(read));
				if (!output.good())
				{
					error = "Failed to write " + destination.u8string();
					return false;
				}

				written += read;
				if (total > 0)
				{
					const int percent = static_cast<int>(written * 100 / total);
					if (percent != last_percent && (percent % 5 == 0 || percent == 100))
					{
						last_percent = percent;
						Report(progress, "Downloading the engine SDK... " +
							std::to_string(percent) + "%");
					}
				}
			}

			output.flush();
			if (!output.good())
			{
				error = "Failed to write " + destination.u8string();
				return false;
			}
			return true;
		}

		/// Extracts a zip archive with the tar tool that ships with Windows.
		bool ExtractPackage(const std::filesystem::path &archive,
			const std::filesystem::path &destination, std::string &error)
		{
			wchar_t system_directory[MAX_PATH]{};
			if (GetSystemDirectoryW(system_directory, MAX_PATH) == 0)
			{
				error = LastErrorMessage("Failed to locate the Windows system directory");
				return false;
			}
			const std::filesystem::path tar =
				std::filesystem::path(system_directory) / L"tar.exe";
			std::error_code filesystem_error;
			if (!std::filesystem::is_regular_file(tar, filesystem_error))
			{
				error = "The Windows tar tool is not available to extract the engine SDK";
				return false;
			}

			std::filesystem::remove_all(destination, filesystem_error);
			std::filesystem::create_directories(destination, filesystem_error);
			if (filesystem_error)
			{
				error = "Failed to create " + destination.u8string() + ": " +
					filesystem_error.message();
				return false;
			}

			std::wstring command = L"\"" + tar.wstring() + L"\" -xf \"" +
				archive.wstring() + L"\" -C \"" + destination.wstring() + L"\"";

			STARTUPINFOW startup{};
			startup.cb = sizeof(startup);
			startup.dwFlags = STARTF_USESHOWWINDOW;
			startup.wShowWindow = SW_HIDE;
			PROCESS_INFORMATION process{};
			if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE,
					CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process))
			{
				error = LastErrorMessage("Failed to start the engine SDK extractor");
				return false;
			}

			WaitForSingleObject(process.hProcess, INFINITE);
			DWORD exit_code = 1;
			GetExitCodeProcess(process.hProcess, &exit_code);
			CloseHandle(process.hThread);
			CloseHandle(process.hProcess);

			if (exit_code != 0)
			{
				std::filesystem::remove_all(destination, filesystem_error);
				error = "Extracting the engine SDK failed (tar exit code " +
					std::to_string(exit_code) + ")";
				return false;
			}
			return true;
		}

		/// Returns the directory that contains the running executable.
		std::filesystem::path GetExecutableDirectory()
		{
			std::wstring buffer(MAX_PATH, L'\0');
			for (;;)
			{
				const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
					static_cast<DWORD>(buffer.size()));
				if (length == 0)
					return {};
				if (length < buffer.size())
				{
					buffer.resize(length);
					return std::filesystem::path(buffer).parent_path();
				}
				buffer.resize(buffer.size() * 2);
			}
		}

	} // namespace

	std::filesystem::path EngineSdk::GetCacheRoot()
	{
		PWSTR local_app_data = nullptr;
		if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr,
				&local_app_data)))
		{
			const std::filesystem::path root =
				std::filesystem::path(local_app_data) / "BlackLightEditor" / "Engine";
			CoTaskMemFree(local_app_data);
			return root;
		}
		return GetExecutableDirectory() / "Engine";
	}

	bool EngineSdk::IsPackage(const std::filesystem::path &root)
	{
		if (root.empty())
			return false;
		std::error_code error;
		return std::filesystem::is_regular_file(root / "include" / "BlackLightApi.h",
				   error) &&
			!error &&
			std::filesystem::is_regular_file(root / "blacklight_engine_x64_static.lib",
				error) &&
			!error;
	}

	EngineSdkPackage EngineSdk::FindNewestCached()
	{
		EngineSdkPackage newest;
		std::error_code error;
		const std::filesystem::path root = GetCacheRoot();
		for (std::filesystem::directory_iterator it(root, error), end; it != end;
			it.increment(error))
		{
			if (error)
				break;
			if (!it->is_directory(error) || error)
				continue;
			const std::string tag = it->path().filename().u8string();
			if (tag.empty() || !IsPackage(it->path()))
				continue;
			if (newest.tag.empty() || tag > newest.tag)
				newest = {tag, it->path()};
		}
		return newest;
	}

	bool EngineSdk::EnsureLatest(EngineSdkPackage &package, std::string &error,
		const ProgressHandler &progress, const CancelCheck &cancel)
	{
		package = {};

		const std::filesystem::path cache_root = GetCacheRoot();
		std::error_code filesystem_error;
		std::filesystem::create_directories(cache_root, filesystem_error);
		if (filesystem_error)
		{
			error = "Failed to create " + cache_root.u8string() + ": " +
				filesystem_error.message();
			return false;
		}

		Report(progress, "Checking the latest BlackLight release...");
		std::string tag;
		std::wstring download_url;
		std::string resolve_error;
		if (!ResolveLatestRelease(tag, download_url, resolve_error))
		{
			const EngineSdkPackage cached = FindNewestCached();
			if (!cached.tag.empty())
			{
				Report(progress, "Offline: using the cached engine SDK " + cached.tag);
				package = cached;
				return true;
			}
			error = resolve_error;
			return false;
		}

		const std::filesystem::path root = cache_root / tag;
		if (IsPackage(root))
		{
			Report(progress, "Using the cached engine SDK " + tag);
			package = {tag, root};
			return true;
		}

		Report(progress, "Downloading the engine SDK " + tag + "...");
		const std::filesystem::path archive = cache_root / (tag + ".zip");
		std::string failure;
		if (!DownloadFile(download_url, archive, failure, progress, cancel))
		{
			std::filesystem::remove(archive, filesystem_error);
			if (cancel && cancel())
			{
				error = "The engine download was cancelled";
				return false;
			}
		}
		else
		{
			Report(progress, "Extracting the engine SDK " + tag + "...");
			std::string extract_error;
			if (ExtractPackage(archive, root, extract_error))
			{
				if (IsPackage(root))
				{
					std::filesystem::remove(archive, filesystem_error);
					package = {tag, root};
					Report(progress, "Engine SDK " + tag + " is ready");
					return true;
				}
				failure = "The downloaded engine SDK package is incomplete";
			}
			else
			{
				failure = extract_error;
			}
			std::filesystem::remove(archive, filesystem_error);
			std::filesystem::remove_all(root, filesystem_error);
		}

		const EngineSdkPackage cached = FindNewestCached();
		if (!cached.tag.empty())
		{
			Report(progress, "Using the cached engine SDK " + cached.tag +
				" after a download failure");
			package = cached;
			return true;
		}

		error = failure;
		return false;
	}

	EngineSdkInstaller::~EngineSdkInstaller()
	{
		if (worker_.joinable())
		{
			cancelled_.store(true);
			worker_.join();
		}
	}

	void EngineSdkInstaller::Start()
	{
		if (IsRunning())
			return;
		if (worker_.joinable())
			worker_.join();

		{
			std::lock_guard<std::mutex> lock(mutex_);
			status_ = "Preparing the engine SDK...";
			package_ = {};
			error_.clear();
		}
		reported_ = false;
		cancelled_.store(false);
		state_.store(State::Running);

		worker_ = std::thread([this]
		{
			EngineSdkPackage package;
			std::string error;
			const bool ready = EngineSdk::EnsureLatest(package, error,
				[this](std::string_view message)
				{
					std::lock_guard<std::mutex> lock(mutex_);
					status_ = std::string(message);
				},
				[this] { return cancelled_.load(); });

			{
				std::lock_guard<std::mutex> lock(mutex_);
				package_ = std::move(package);
				error_ = std::move(error);
				if (!ready && status_.empty())
					status_ = "The engine SDK could not be prepared";
			}
			state_.store(ready ? State::Ready : State::Failed);
		});
	}

	void EngineSdkInstaller::Cancel() { cancelled_.store(true); }

	bool EngineSdkInstaller::Poll()
	{
		const State state = state_.load();
		if (state != State::Ready && state != State::Failed)
			return false;
		if (worker_.joinable())
			worker_.join();
		if (reported_)
			return false;
		reported_ = true;
		return true;
	}

	bool EngineSdkInstaller::IsRunning() const noexcept
	{
		return state_.load() == State::Running;
	}

	EngineSdkInstaller::State EngineSdkInstaller::GetState() const noexcept
	{
		return state_.load();
	}

	std::string EngineSdkInstaller::GetStatus() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return status_;
	}

	const EngineSdkPackage &EngineSdkInstaller::GetPackage() const noexcept
	{
		return package_;
	}

	const std::string &EngineSdkInstaller::GetError() const noexcept { return error_; }

} // namespace BlackLight::Editor
