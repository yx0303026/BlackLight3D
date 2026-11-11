#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

namespace BlackLight::Editor
{

	/// Identifies an extracted engine SDK package published on the BlackLight GitHub releases.
	struct EngineSdkPackage
	{
		/// Release tag such as `v0.0.1`; empty when no package is available.
		std::string tag;
		/// Directory that contains `include/` and the prebuilt engine libraries.
		std::filesystem::path root;
	};

	/// Resolves, downloads, and extracts the prebuilt engine SDK from GitHub releases.
	///
	/// Packages are cached per release tag below `%LOCALAPPDATA%/BlackLightEditor/Engine`
	/// so a project can be generated without touching the engine source tree.
	class EngineSdk
	{
	public:
		/// Receives human-readable progress lines while a package is prepared.
		using ProgressHandler = std::function<void(std::string_view)>;
		/// Returns true when the pending operation should stop.
		using CancelCheck = std::function<bool()>;

		/// GitHub repository that publishes the engine SDK.
		static constexpr std::string_view Repository = "yx0303026/BlackLight3D";
		/// Release asset downloaded by the editor.
		static constexpr std::string_view AssetName = "blacklight_engine_x64_release.zip";

		/// Returns the directory that caches extracted SDK packages.
		static std::filesystem::path GetCacheRoot();

		/// Returns whether the directory holds a complete extracted package.
		static bool IsPackage(const std::filesystem::path &root);

		/// Returns the newest cached package, or an empty package when the cache is empty.
		static EngineSdkPackage FindNewestCached();

		/// Downloads and extracts the latest release unless its tag is already cached.
		/// Blocking; run it on a worker thread to keep the editor responsive.
		static bool EnsureLatest(EngineSdkPackage &package, std::string &error,
			const ProgressHandler &progress = {}, const CancelCheck &cancel = {});
	};

	/// Runs `EngineSdk::EnsureLatest` on a worker thread and exposes its state.
	class EngineSdkInstaller
	{
	public:
		enum class State
		{
			Idle,
			Running,
			Ready,
			Failed
		};

		EngineSdkInstaller() = default;
		~EngineSdkInstaller();

		EngineSdkInstaller(const EngineSdkInstaller &) = delete;
		EngineSdkInstaller &operator=(const EngineSdkInstaller &) = delete;

		/// Starts a new installation; ignored while a previous run is active.
		void Start();
		/// Requests cancellation of the active run.
		void Cancel();
		/// Joins a finished worker and reports the state transition exactly once.
		bool Poll();
		/// Returns the current installer state.
		State GetState() const noexcept;
		/// Returns whether a worker is still preparing the package.
		bool IsRunning() const noexcept;
		/// Returns the latest progress line.
		std::string GetStatus() const;
		/// Returns the package produced by the last successful run.
		const EngineSdkPackage &GetPackage() const noexcept;
		/// Returns the failure reported by the last run.
		const std::string &GetError() const noexcept;

	private:
		std::thread worker_;
		std::atomic<State> state_{State::Idle};
		std::atomic<bool> cancelled_{false};
		mutable std::mutex mutex_;
		std::string status_;
		EngineSdkPackage package_;
		std::string error_;
		bool reported_ = false;
	};

} // namespace BlackLight::Editor
