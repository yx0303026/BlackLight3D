# BlackLight3D HTML+CSS UI Parser Guide

## Overview

BlackLight3D now provides full HTML+CSS parsing support through the new `HtmlCssParser` class. This allows you to define UI layouts using standard HTML and CSS, which are automatically converted to RmlUi-compatible elements and styles.

## Features

### Supported HTML Elements

- **Container Elements**: `div`, `section`, `article`, `nav`, `aside`, `header`, `footer`, `main`
- **Interactive Elements**: `button`, `select` (maps to ListBox), `table` (maps to DataGrid)
- **Semantic Elements**: `p`, `blockquote`, `ul`, `ol`, `li`, `form`
- **Text Elements**: `span`, `a`, `strong`, `em`, `b`, `i`, etc.
- **Inline Elements**: All standard inline HTML elements

### Supported CSS Properties

#### Layout & Spacing
- `width`, `height` - Element dimensions (px, %, em, rem, etc.)
- `padding` - Internal spacing
- `margin` - External spacing
- `display` - Block, inline, flex, etc.
- `position` - relative, absolute, fixed
- `top`, `left`, `right`, `bottom` - Positioning offsets
- `gap` - Space between flex items
- `flex-direction`, `flex-wrap` - Flex layout properties
- `justify-content`, `align-items` - Flex alignment

#### Typography
- `font-size` - Text size
- `font-family` - Font selection
- `font-weight` - Font boldness
- `font-style` - Italic or normal
- `color` - Text color
- `text-align` - Alignment (left, center, right)
- `text-decoration` - Underline, strikethrough, etc.
- `line-height` - Line spacing

#### Visual Effects
- `background-color` - Background color
- `border` - Border definition
- `border-radius` - Rounded corners
- `opacity` - Transparency
- `box-shadow` - Shadow effects
- `z-index` - Stacking order

#### Color Formats Supported
- Hex: `#FF0000`, `#F00`, `#FF0000FF`
- RGB: `rgb(255, 0, 0)`
- RGBA: `rgba(255, 0, 0, 0.5)`
- Named colors: `red`, `blue`, `white`, etc.

## Usage

### Basic Usage

```cpp
#include "rmlui/HtmlCssParser.h"

// Parse HTML with optional CSS
std::unique_ptr<UIScreen> screen;
HTMLParseError error;

std::string html = R"(
    <div id="main" style="width: 100%; height: 200px; background-color: #FFFFFF;">
        <button id="my_button" style="width: 100px; height: 40px;">Click Me</button>
    </div>
)";

bool success = HtmlCssParser::Parse(html, "", screen, error);

if (!success) {
    std::cerr << "Parse error: " << error.message << " at line " << error.line << std::endl;
} else {
    // Use the screen with RmlUi
    RmlUiTree tree;
    // ... build tree ...
}
```

### Parsing from Files

```cpp
std::unique_ptr<UIScreen> screen;
HTMLParseError error;

bool success = HtmlCssParser::ParseFromFiles(
    "ui/layout.html",
    "ui/styles.css",
    screen,
    error
);
```

### With External CSS

```cpp
std::string html = R"(
    <div id="container" class="main-layout">
        <div class="card">Content here</div>
    </div>
)";

std::string css = R"(
    .main-layout {
        width: 100%;
        height: 100%;
        display: flex;
        flex-direction: column;
    }
    
    .card {
        background-color: white;
        border-radius: 8px;
        padding: 20px;
        margin: 10px;
        box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
    }
)";

HtmlCssParser::Parse(html, css, screen, error);
```

## CSS to RmlUi Mapping

The parser automatically maps standard CSS to RmlUi-compatible properties:

| CSS Property | RmlUi Equivalent | Notes |
|--------------|------------------|-------|
| `width` | `width` | Normalized units (px preferred) |
| `height` | `height` | Normalized units (px preferred) |
| `background-color` | `background-color` | Supports hex, rgb, rgba, named colors |
| `border` | `border` | Full border syntax supported |
| `border-radius` | `border-radius` | For rounded corners |
| `color` | `color` | Text color |
| `font-size` | `font-size` | Normalized to px or em |
| `font-family` | `font-family` | System fonts or custom |
| `display: flex` | Flex layout | RmlUi native flex support |
| `padding` | `padding` | All margin/padding shortcuts |
| `margin` | `margin` | All margin shortcuts |

## Example: Complete UI Layout

```html
<!DOCTYPE html>
<html>
<head>
    <style>
        body {
            width: 100%;
            height: 100%;
            margin: 0;
            font-family: Arial, sans-serif;
        }
        
        #header {
            width: 100%;
            height: 60px;
            background-color: #333333;
            color: white;
            display: flex;
            align-items: center;
            padding: 10px;
            box-sizing: border-box;
        }
        
        #content {
            display: flex;
            width: 100%;
            height: calc(100% - 120px);
        }
        
        #sidebar {
            width: 200px;
            background-color: #F5F5F5;
            padding: 20px;
            border-right: 1px solid #CCCCCC;
        }
        
        #main {
            flex: 1;
            padding: 20px;
            overflow-y: auto;
        }
        
        .card {
            background-color: white;
            border: 1px solid #E0E0E0;
            border-radius: 8px;
            padding: 15px;
            margin-bottom: 15px;
            box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
        }
        
        .btn {
            padding: 10px 20px;
            margin: 5px;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-weight: bold;
        }
        
        .btn-primary {
            background-color: #3498DB;
            color: white;
        }
        
        #footer {
            width: 100%;
            height: 40px;
            background-color: #333333;
            color: white;
            display: flex;
            align-items: center;
            justify-content: center;
        }
    </style>
</head>
<body>
    <div id="header">Welcome to BlackLight3D</div>
    
    <div id="content">
        <div id="sidebar">
            <div style="font-weight: bold; margin-bottom: 10px;">Menu</div>
            <div style="padding: 5px; margin: 5px 0;">Dashboard</div>
            <div style="padding: 5px; margin: 5px 0;">Settings</div>
            <div style="padding: 5px; margin: 5px 0;">Help</div>
        </div>
        
        <div id="main">
            <div class="card">
                <div style="font-weight: bold; font-size: 18px; margin-bottom: 10px;">
                    Welcome
                </div>
                <div>This is a sample layout created with HTML and CSS.</div>
            </div>
            
            <div class="card">
                <button class="btn btn-primary" id="action_btn">Action</button>
            </div>
        </div>
    </div>
    
    <div id="footer">© 2024 BlackLight3D</div>
</body>
</html>
```

## Important Notes

### Unit Normalization
- Unitless numbers are automatically converted to `px` (e.g., `width: 100` → `width: 100px`)
- Supported units: `px`, `%`, `em`, `rem`, `pt`, `cm`, `mm`, `in`, `ex`, `ch`, `vw`, `vh`
- RmlUi primarily uses `px` and `%`, so convert other units if needed

### Class Attributes
While the parser recognizes the `class` attribute, external CSS class selectors are stored in the stylesheet and must be resolved by RmlUi. For immediate styling, use inline `style` attributes.

### ID Uniqueness
All element IDs must be unique within a document. The parser validates this and returns an error if duplicates are found.

### Void Elements
Self-closing elements like `<br>`, `<img>`, `<input>` are properly handled without requiring closing tags.

### Comments
HTML comments (`<!-- comment -->`) are skipped during parsing.

## Error Handling

The parser provides detailed error information:

```cpp
HTMLParseError error;
if (!HtmlCssParser::Parse(html, css, screen, error)) {
    std::cerr << "Error at line " << error.line << ": " 
              << error.message << std::endl;
}
```

### Common Errors
- Empty HTML content
- Duplicate element IDs
- Invalid CSS dimension units
- Malformed HTML structure
- Missing required attributes

## Performance Considerations

- **Large Documents**: For HTML > 100KB, consider splitting into multiple smaller layouts
- **CSS Parsing**: External CSS is stored as-is; complex selectors should be minimized
- **Nested Elements**: Deep nesting (>20 levels) may impact rendering performance

## Integration with Existing XML Parser

The HTML+CSS parser works alongside the existing XML parser:

```cpp
// For XML-based layouts (existing)
bool xml_success = UIXmlParser::Parse(xml_content, screen, xml_error);

// For HTML+CSS layouts (new)
bool html_success = HtmlCssParser::Parse(html_content, css_content, screen, html_error);
```

## Contributing Custom Elements

To add support for custom HTML elements:

1. Add to `ELEMENT_KIND_MAP` in HtmlCssParser.cpp
2. Update `GetElementKind()` function
3. Map to appropriate `UIElementKind`
4. Update corresponding `ParseHTMLElement()` logic

## Future Enhancements

Planned features for future releases:
- CSS selector support (.class, #id, element selectors)
- Media queries
- CSS animations
- SVG element support
- WebFont support
- Responsive design helpers

## Examples

See `Demo/UI/demo.html` for a complete, working example of an HTML+CSS UI layout.

## Troubleshooting

### "HTML content is empty"
Ensure you're passing non-empty HTML string to `Parse()` or verify the file exists for `ParseFromFiles()`.

### "Control identifiers must be unique"
Check that no two elements have the same `id` attribute.

### "Invalid CSS dimension"
Verify dimension values include a valid unit (px, %, em, etc.).

### Styles not appearing in RmlUi
- For external CSS, ensure it's passed to `Parse()` as the second parameter
- For inline styles, check syntax of the `style` attribute
- Verify RmlUi supports the CSS properties being used

## Support

For issues or feature requests, see the BlackLight3D documentation or contact the development team.
