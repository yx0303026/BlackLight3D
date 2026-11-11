#pragma once

#include "editor/core/EditorTheme.h"
#include "rmlui/BuiTree.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Rml
{
	class Context;
	class Element;
	class ElementDocument;
	class Event;
}

namespace BlackLight::Editor
{

	/// Owns the editor's RmlUi document and provides the BlackLightUI integration layer.
	///
	/// The layer owns generated listeners so callers can bind to elements by id
	/// without managing RmlUi listener lifetimes. Listeners are grouped by scope,
	/// which lets dynamic regions be regenerated without leaking stale callbacks.
	class EditorUi final
	{
	public:
		/// Receives one native RmlUi event.
		using EventHandler = std::function<void(Rml::Event &)>;

		EditorUi();
		~EditorUi();

		EditorUi(const EditorUi &) = delete;
		EditorUi &operator=(const EditorUi &) = delete;

		/// Parses and builds the embedded `.bui` document with the supplied theme applied.
		bool Initialize(Rml::Context &context, std::string_view bui_content,
			const EditorTheme &theme, std::string &error);
		/// Removes listeners and closes the document while its context is alive.
		void Shutdown();
		/// Regenerates the document stylesheet for a new theme without rebuilding the tree.
		void SetTheme(const EditorTheme &theme);

		/// Returns whether a document is currently loaded.
		bool IsReady() const noexcept;
		/// Returns the owned document, or null after shutdown.
		Rml::ElementDocument *GetDocument() const noexcept;

		/// Finds an element by id, including elements created at runtime.
		Rml::Element *Find(std::string_view id) const;
		/// Replaces an element's text with HTML-escaped content.
		void SetText(std::string_view id, std::string_view text);
		/// Replaces an element's inner RML with the supplied markup.
		void SetHtml(std::string_view id, std::string_view html);
		/// Adds or removes one CSS class on an element.
		void SetClass(std::string_view id, std::string_view class_name, bool active);
		/// Shows or hides an element through the shared `hidden` class.
		void SetVisible(std::string_view id, bool visible);
		/// Sets one inline style property.
		void SetProperty(std::string_view id, std::string_view property, std::string_view value);

		/// Binds a listener to an element id within a removable scope.
		bool AddListener(std::string_view scope, std::string_view element_id,
			std::string_view event, EventHandler handler);
		/// Binds a listener to a resolved element within a removable scope.
		void AddListener(std::string_view scope, Rml::Element *element,
			std::string_view event, EventHandler handler);
		/// Detaches every listener registered under a scope.
		void ClearListeners(std::string_view scope);
		/// Detaches every registered listener.
		void ClearListeners();

		/// Escapes text for use in generated RML text and double-quoted attributes.
		static std::string Escape(std::string_view text);
		/// Returns the nearest attribute value at or above an element below a container.
		static std::string FindAttribute(Rml::Element *element, Rml::Element *stop,
			std::string_view name);

	private:
		class CallbackListener;

		struct ListenerEntry
		{
			std::string scope;
			Rml::Element *element = nullptr;
			std::string event;
			std::unique_ptr<CallbackListener> listener;
		};

		std::unique_ptr<BlackLight::UI::BuiTree> tree_;
		std::string base_styles_;
		std::vector<ListenerEntry> listeners_;
	};

} // namespace BlackLight::Editor
