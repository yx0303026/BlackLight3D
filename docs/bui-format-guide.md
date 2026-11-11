# BlackLight3D .bui File Format

## Overview

The `.bui` (BlackLight UI) file format is a lightweight XML container for RmlUi-native HTML and CSS. It replaces the need for separate HTML parsers by using RmlUi's built-in HTML+CSS system directly.

**Why .bui?**
- ✨ Eliminates the overhead of HTML parsing and interpretation
- 🚀 Uses RmlUi's native element system directly
- 📝 Simple XML structure with two clear sections
- ⚡ Faster load times and lower memory usage

## File Structure

A `.bui` file is an XML file with exactly two sections:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<bui>
  <styles>
    /* RmlUi CSS code */
  </styles>
  <layout>
    /* RmlUi HTML code */
  </layout>
</bui>
```

## Sections

### `<styles>` Section

Contains all CSS stylesheets for the UI. This is pure RmlUi CSS that will be loaded into a stylesheet container.

**Example:**
```xml
<styles>
  body {
    display: block;
    width: 100%;
    height: 100%;
    background-color: #263746;
    font-family: Arial;
    color: #f4f6f8;
  }

  button {
    background-color: #326b8c;
    padding: 7px 12px;
    color: #ffffff;
    cursor: pointer;
  }

  button:hover {
    background-color: #3f829f;
  }

  .panel {
    display: flex;
    flex-direction: column;
    gap: 8px;
  }
</styles>
```

**Notes:**
- All CSS must be RmlUi-compatible (not all CSS3 features are supported)
- Inline `<style>` tags inside the layout are NOT processed; put all CSS in the `<styles>` section
- Multiple selectors, pseudo-classes, and media queries are supported

### `<layout>` Section

Contains the RmlUi HTML that forms the UI structure. This is loaded directly into RmlUi's DOM.

**Example:**
```xml
<layout>
  <div id="main_screen" class="main-screen">
    <div id="title" class="title-panel">
      <div class="title-text">My App</div>
      <button id="start_button">Start</button>
    </div>
    
    <div id="content" class="content-panel">
      <div id="left_panel">Left Content</div>
      <div id="right_panel">Right Content</div>
    </div>
  </div>
</layout>
```

**Supported HTML Elements:**
- `div`, `span`, `p`, `h1`-`h6`
- `button`, `input`, `textarea`, `select`, `label`
- `a`, `img`
- `ul`, `ol`, `li`
- `table`, `tr`, `td`, `th`
- Custom elements (mapped to RmlUi elements)

## Usage in C++

### Loading and Building a .bui File

```cpp
#include "rmlui/BuiLoader.h"
#include "rmlui/BuiTree.h"

// Load .bui file
std::unique_ptr<BuiDocument> bui_document;
BuiLoadError load_error;

if (!BuiLoader::LoadFromFile("UI/Main.bui", bui_document, load_error)) {
    std::cerr << "Error: " << load_error.message << " (line " << load_error.line << ")" << std::endl;
    return false;
}

// Build RmlUi tree
Rml::Context* context = GetRmlUiContext(); // Your RmlUi context
std::unique_ptr<BuiTree> ui_tree;
BuiBuildError build_error;

if (!BuiTree::Build(*context, *bui_document, ui_tree, build_error)) {
    std::cerr << "Build error: " << build_error.message << std::endl;
    return false;
}

// Show the UI
ui_tree->Show();
```

### Accessing Elements

After building, you can find and interact with elements by their IDs:

```cpp
// Find an element by ID
if (auto button = ui_tree->FindById("start_button")) {
    // button is an Rml::Element*
    // Use RmlUi API to interact with it
}
```

### Cleaning Up

When you're done:

```cpp
ui_tree->Close();
// ui_tree will be destroyed when the unique_ptr goes out of scope
```

## Design Guidelines

### ID and Class Naming

Use descriptive, snake_case names for IDs and classes:

```xml
<button id="start_game_button" class="primary-button">Start</button>
<div id="asset_panel" class="side-panel">Assets</div>
```

❌ Avoid:
```xml
<button id="btn1" class="b">Start</button>
```

### CSS Organization

Group related styles by component:

```xml
<styles>
  /* Layout */
  .main-container { display: flex; }
  .panel { flex: 1; }

  /* Typography */
  .title { font-size: 24px; font-weight: bold; }
  .subtitle { font-size: 16px; }

  /* Colors */
  .primary { background-color: #326b8c; }
  .secondary { background-color: #34495e; }

  /* Interactive */
  button:hover { background-color: #3f829f; }
  input:focus { border-color: #2ecc71; }
</styles>
```

### Responsive Layouts

Use Flexbox for responsive layouts:

```xml
<styles>
  .content-panel {
    display: flex;
    flex-direction: row;
    gap: 12px;
  }

  .panel-half {
    flex: 1;
    min-width: 0;
  }

  @media (max-width: 800px) {
    .content-panel {
      flex-direction: column;
    }
  }
</styles>
```

## Common Patterns

### Modal Dialog

```xml
<div id="modal" class="modal" style="display: none;">
  <div class="modal-content">
    <h2>Confirm</h2>
    <p>Are you sure?</p>
    <div class="button-group">
      <button id="confirm">Yes</button>
      <button id="cancel">No</button>
    </div>
  </div>
</div>
```

### Button Group

```xml
<div class="button-group">
  <button id="btn_new">New</button>
  <button id="btn_open">Open</button>
  <button id="btn_save">Save</button>
</div>

<styles>
  .button-group {
    display: flex;
    flex-direction: row;
    gap: 8px;
  }
</styles>
```

### List with Items

```xml
<div class="list-container">
  <div class="list-header">Items</div>
  <div class="list-box">
    <div class="list-item">Item 1</div>
    <div class="list-item">Item 2</div>
    <div class="list-item">Item 3</div>
  </div>
</div>

<styles>
  .list-box {
    display: flex;
    flex-direction: column;
    gap: 4px;
    overflow-y: auto;
  }

  .list-item {
    padding: 8px;
    background-color: #34495e;
    cursor: pointer;
  }

  .list-item:hover {
    background-color: #3f5268;
  }
</styles>
```

### Two-Panel Layout

```xml
<div class="main-container">
  <div class="left-panel">
    <div class="panel-title">Resources</div>
    <div class="resource-list">
      <!-- List items -->
    </div>
  </div>
  
  <div class="right-panel">
    <div class="panel-title">Details</div>
    <div class="resource-details">
      <!-- Details content -->
    </div>
  </div>
</div>

<styles>
  .main-container {
    display: flex;
    flex-direction: row;
    height: 100%;
    gap: 12px;
  }

  .left-panel {
    width: 250px;
    display: flex;
    flex-direction: column;
  }

  .right-panel {
    flex: 1;
    display: flex;
    flex-direction: column;
  }
</styles>
```

## RmlUi CSS Reference

Not all CSS properties are supported in RmlUi. Here are the commonly used ones:

### Layout Properties
- `display` (block, inline, flex, none)
- `flex-direction` (row, column)
- `justify-content`, `align-items`, `align-content`
- `flex`, `flex-grow`, `flex-shrink`, `flex-basis`
- `gap`, `row-gap`, `column-gap`

### Box Model
- `width`, `height`, `min-width`, `max-width`, `min-height`, `max-height`
- `padding`, `padding-top`, `padding-right`, `padding-bottom`, `padding-left`
- `margin`, `margin-top`, `margin-right`, `margin-bottom`, `margin-left`
- `border`, `border-width`, `border-color`, `border-style`
- `border-radius`
- `box-sizing` (border-box, content-box)

### Visual Properties
- `background-color`
- `color`
- `opacity`
- `visibility`
- `overflow`, `overflow-x`, `overflow-y`

### Typography
- `font-family`, `font-size`, `font-weight`, `font-style`
- `line-height`, `text-align`, `text-decoration`

### Positioning & Floats
- `position` (static, absolute, relative, fixed)
- `top`, `right`, `bottom`, `left`
- `z-index`
- `float` (left, right, none)
- `clear`

### Effects
- `box-shadow`
- `text-shadow`

### Transitions & Transforms
- `transition`, `transition-property`, `transition-duration`, `transition-timing-function`
- `transform`

## Migration from HTML Parser

If you're migrating from the old HTML+CSS parser system:

### Before (HTMLParser):
```cpp
#include "rmlui/HtmlCssParser.h"

std::unique_ptr<HTMLDocument> document;
HTMLParseError error;
HtmlCssParser::ParseFromFiles("UI/Main.html", "UI/Main.css", document, error);
RmlUiTree::Build(*context, *document, ui_tree, build_error);
```

### After (.bui):
```cpp
#include "rmlui/BuiLoader.h"
#include "rmlui/BuiTree.h"

std::unique_ptr<BuiDocument> bui_document;
BuiLoadError error;
BuiLoader::LoadFromFile("UI/Main.bui", bui_document, error);
BuiTree::Build(*context, *bui_document, ui_tree, build_error);
```

## Error Handling

### Load Errors

```cpp
if (!BuiLoader::LoadFromFile("UI/Main.bui", bui_document, load_error)) {
    std::cout << "Error: " << load_error.message << std::endl;
    if (load_error.line > 0) {
        std::cout << "At line: " << load_error.line << std::endl;
    }
}
```

Common load errors:
- "Failed to open file" - File doesn't exist or permission denied
- "Missing <styles> section" - No `<styles>` tag found
- ".bui content is empty" - File is empty
- "Unterminated tag" - Malformed XML

### Build Errors

```cpp
if (!BuiTree::Build(*context, *bui_document, ui_tree, build_error)) {
    std::cout << "Build error: " << build_error.message << std::endl;
}
```

Common build errors:
- "Failed to create RmlUi document" - RmlUi context issue
- "Failed to load CSS stylesheet" - Invalid CSS syntax
- "Failed to load layout HTML" - Invalid HTML syntax

## Performance Tips

1. **Keep CSS organized** - Use selectors efficiently
2. **Minimize DOM depth** - Shallow hierarchies render faster
3. **Use CSS classes** - More efficient than inline styles
4. **Avoid excessive nesting** - Keep Flexbox levels reasonable
5. **Element IDs for lookup** - Use FindById() only for frequently accessed elements

## Debugging

### Verify File Format

```xml
<?xml version="1.0" encoding="UTF-8"?>
<bui>
  <styles>
    /* CSS here */
  </styles>
  <layout>
    /* HTML here */
  </layout>
</bui>
```

### Check RmlUi Logs

Enable RmlUi debug output to see rendering issues:
```cpp
Rml::SetSystemInterface(new Rml::SystemInterface());
```

### Validate XML

Ensure your .bui file is well-formed XML:
- All tags are properly closed
- Attribute values are quoted
- Special characters are escaped

## Examples

See `Demo/UI/Main.bui` for a complete example with:
- Two-panel layout
- Styled buttons
- Responsive grid
- Status bar
- Theme colors

## API Reference

### BuiLoader

```cpp
static bool LoadFromFile(std::string_view file_path,
    std::unique_ptr<BuiDocument>& document,
    BuiLoadError& error);

static bool Parse(std::string_view bui_content,
    std::unique_ptr<BuiDocument>& document,
    BuiLoadError& error);
```

### BuiDocument

```cpp
const std::string& GetLayout() const noexcept;
const std::string& GetStyles() const noexcept;
void SetLayout(std::string layout);
void SetStyles(std::string styles);
```

### BuiTree

```cpp
static bool Build(Rml::Context& context,
    const BuiDocument& bui_document,
    std::unique_ptr<BuiTree>& tree,
    BuiBuildError& error);

Rml::ElementDocument* GetDocument() const noexcept;
Rml::Element* FindById(std::string_view id) const;
void Show();
void Close();
```

## Changelog

### Version 1.0 (2024)
- Initial .bui format release
- Direct RmlUi HTML+CSS loading
- Element ID mapping
- Error handling with line numbers
