# BlackLight3D HTML+CSS Parser Guide

## Overview

BlackLight3D now includes **HtmlCssParser** - a powerful module that allows you to define UI using standard HTML+CSS syntax instead of RmlUI's custom XML format. The parser automatically converts HTML/CSS to RmlUI-compatible elements and styles.

## Features

- **HTML Parsing**: Support for standard HTML elements (div, button, span, etc.)
- **CSS Support**: Both inline styles and external CSS stylesheets
- **Automatic Conversion**: Seamless translation from CSS to RmlUI properties
- **Layout Systems**:
  - Flexbox (with `display: flex`, `flex-direction`, `gap`, `flex` properties)
  - Grid (basic support)
  - Standard box model (width, height, padding, margin)
- **Style Properties**:
  - Colors and backgrounds (`background-color`, `color`)
  - Text styling (`font-size`, `text-align`, `font-weight`)
  - Borders (`border`, `border-radius`)
  - Effects (`opacity`, `box-shadow`)
  - Spacing (`padding`, `margin`)
  - Display (`display`, `visibility`)
  - Positioning (`position`, `top`, `left`, `right`, `bottom`, `z-index`)

## Basic Usage

### Example 1: Simple HTML Layout

```cpp
#include "rmlui/HtmlCssParser.h"
#include <memory>

// Define your HTML
std::string html = R"(
    <div id="main_container" style="width: 100%; height: 100%; background-color: #FFFFFF;">
        <div id="header" style="width: 100%; height: 60px; background-color: #333333;">
            <h1 style="color: white; font-size: 24px; text-align: center;">My App</h1>
        </div>
        <div id="content" style="width: 100%; height: calc(100% - 60px); padding: 20px;">
            <p style="color: #333333; font-size: 14px;">Welcome!</p>
        </div>
    </div>
)";

// Parse the HTML
std::unique_ptr<BlackLight::UI::UIScreen> screen;
BlackLight::UI::HTMLParseError error;

if (BlackLight::UI::HtmlCssParser::Parse(html, "", screen, error)) {
    // Successfully parsed!
    // Use the screen in your UI rendering
    RenderScreen(screen.get());
} else {
    // Handle error
    std::cerr << "Parse error: " << error.message << " at line " << error.line << std::endl;
}
```

### Example 2: Using External CSS

```cpp
// Define CSS separately
std::string css = R"(
    .container {
        width: 100%;
        height: 100%;
        display: flex;
        flex-direction: column;
        gap: 10px;
        padding: 20px;
        background-color: #F5F5F5;
    }
    
    .header {
        width: 100%;
        height: 80px;
        background-color: #2C3E50;
        border-radius: 8px;
        padding: 10px;
        box-sizing: border-box;
    }
    
    .content {
        flex: 1;
        background-color: #FFFFFF;
        border: 1px solid #CCCCCC;
        border-radius: 8px;
        padding: 20px;
        overflow: auto;
    }
    
    .button {
        width: 120px;
        height: 40px;
        background-color: #3498DB;
        color: white;
        font-size: 14px;
        border: none;
        border-radius: 4px;
        cursor: pointer;
    }
    
    .button:hover {
        background-color: #2980B9;
        opacity: 0.9;
    }
)";

// Define HTML using CSS classes
std::string html = R"(
    <div id="app" class="container">
        <div id="header" class="header">
            <h1 style="color: white; margin: 0;">Application Title</h1>
        </div>
        <div id="content" class="content">
            <p>Your content goes here...</p>
        </div>
        <button id="submit_btn" class="button">Submit</button>
    </div>
)";

// Parse with external CSS
std::unique_ptr<BlackLight::UI::UIScreen> screen;
BlackLight::UI::HTMLParseError error;

if (BlackLight::UI::HtmlCssParser::Parse(html, css, screen, error)) {
    // Successfully parsed with CSS applied!
} else {
    std::cerr << "Parse error: " << error.message << std::endl;
}
```

### Example 3: Flexbox Layout

```cpp
std::string html = R"(
    <div id="flex_layout" 
         style="display: flex; 
                 flex-direction: row; 
                 width: 100%; 
                 height: 100%;
                 gap: 10px;
                 padding: 10px;">
        
        <div id="sidebar" 
             style="width: 200px; 
                     background-color: #ECF0F1; 
                     padding: 15px;">
            Sidebar
        </div>
        
        <div id="main" 
             style="flex: 1; 
                     background-color: #FFFFFF; 
                     padding: 15px;">
            Main Content (flexible width)
        </div>
        
        <div id="right_panel" 
             style="width: 150px; 
                     background-color: #ECF0F1; 
                     padding: 15px;">
            Right Panel
        </div>
    </div>
)";

std::unique_ptr<BlackLight::UI::UIScreen> screen;
BlackLight::UI::HTMLParseError error;

if (BlackLight::UI::HtmlCssParser::Parse(html, "", screen, error)) {
    // Flexbox layout parsed successfully
}
```

## Supported HTML Elements

The parser supports the following HTML elements:

| Element | Maps to RmlUI | Notes |
|---------|---------------|-------|
| `<div>` | Panel | Container element |
| `<button>` | Panel (with button styling) | Interactive button |
| `<span>` | Text | Inline text element |
| `<p>` | Panel | Paragraph container |
| `<h1>` to `<h6>` | Text | Heading (larger text) |
| `<input>` | Panel | Input field placeholder |
| `<textarea>` | Panel | Text area placeholder |
| `<img>` | Image | Image element |
| `<ul>`, `<ol>` | Panel | List container |
| `<li>` | Text | List item |

## Supported CSS Properties

### Layout & Positioning
- `display`: flex, block, inline, none
- `flex-direction`: row, column, row-reverse, column-reverse
- `flex`: shorthand for flex-grow, flex-shrink, flex-basis
- `gap`: spacing between flex items
- `justify-content`: flex-start, center, flex-end, space-between, space-around
- `align-items`: flex-start, center, flex-end, stretch
- `position`: absolute, relative, fixed
- `top`, `left`, `right`, `bottom`: positioning offsets
- `z-index`: stacking order
- `width`, `height`: dimensions (px, %, em, rem)
- `min-width`, `max-width`, `min-height`, `max-height`
- `box-sizing`: border-box, content-box

### Spacing
- `padding`: space inside the element
- `margin`: space outside the element
- `gap`: space between flex/grid items

### Backgrounds & Borders
- `background-color`: color value (#RGB, #RRGGBB, rgba, hsl)
- `background-image`: background image URL
- `border`: shorthand for border-width, border-style, border-color
- `border-radius`: corner rounding (px, %)
- `border-color`, `border-width`, `border-style`

### Text Styling
- `color`: text color
- `font-size`: text size (px, em, rem, %)
- `font-weight`: bold, normal, or numeric (100-900)
- `font-family`: font name
- `text-align`: left, center, right, justify
- `text-decoration`: underline, overline, line-through, none
- `font-style`: italic, normal
- `letter-spacing`: space between characters
- `line-height`: space between lines

### Visual Effects
- `opacity`: transparency (0.0 - 1.0)
- `box-shadow`: shadow effect (offset-x offset-y blur spread color)
- `text-shadow`: text shadow effect
- `transform`: scale, rotate, translate
- `filter`: blur, brightness, contrast, grayscale, etc.

### Other
- `overflow`: visible, hidden, auto, scroll
- `visibility`: visible, hidden
- `cursor`: pointer, default, text, etc.

## CSS to RmlUI Property Mapping

The parser automatically converts CSS properties to RmlUI's property format:

| CSS Property | RmlUI Property | Notes |
|--------------|----------------|-------|
| `width: 100px` | `width: 100px` | Direct mapping |
| `background-color: #FF0000` | `background-color: rgb(255, 0, 0)` | Color conversion |
| `font-size: 16px` | `font-size: 16px` | Direct mapping |
| `display: flex` | (internal layout system) | Sets layout mode |
| `flex: 1` | `flex-grow: 1` | Flex expansion |
| `color: white` | `color: white` | Direct mapping |

## Error Handling

The parser provides detailed error information:

```cpp
BlackLight::UI::HTMLParseError error;

if (!BlackLight::UI::HtmlCssParser::Parse(html, css, screen, error)) {
    std::cerr << "Error: " << error.message << std::endl;
    std::cerr << "Line: " << error.line << std::endl;
    std::cerr << "Column: " << error.column << std::endl;
}
```

Common errors include:
- **Empty HTML**: No content provided
- **Duplicate IDs**: Multiple elements with the same ID
- **Invalid CSS**: Malformed CSS syntax
- **Invalid property values**: Out-of-range or unsupported values

## Advanced Usage

### Loading HTML from File

```cpp
#include <fstream>
#include <sstream>

// Load HTML from file
std::ifstream html_file("path/to/ui.html");
std::stringstream html_buffer;
html_buffer << html_file.rdbuf();
std::string html = html_buffer.str();

// Load CSS from file
std::ifstream css_file("path/to/styles.css");
std::stringstream css_buffer;
css_buffer << css_file.rdbuf();
std::string css = css_buffer.str();

// Parse
std::unique_ptr<BlackLight::UI::UIScreen> screen;
BlackLight::UI::HTMLParseError error;
BlackLight::UI::HtmlCssParser::Parse(html, css, screen, error);
```

### Dynamic Style Application

```cpp
// Parse base HTML
std::unique_ptr<BlackLight::UI::UIScreen> screen;
BlackLight::UI::HtmlCssParser::Parse(html, "", screen, error);

// Access element styles
if (auto* element = screen->GetElementById("my_element")) {
    // Element found, can access its properties
    auto layout = element->GetLayout();
    // Modify as needed...
}
```

### Converting from XML

If you have existing RmlUI XML files, you can gradually migrate to HTML+CSS:

**Original XML:**
```xml
<Screen xmlns:c="urn:blacklight:ui" c:id="main_screen">
    <Panel c:id="content" width="100%" height="100%">
        <Panel c:id="header" height="60px" />
        <Panel c:id="body" auto_size="true" />
    </Panel>
</Screen>
```

**Equivalent HTML+CSS:**
```html
<div id="main_screen" style="width: 100%; height: 100%;">
    <div id="content" style="width: 100%; height: 100%;">
        <div id="header" style="width: 100%; height: 60px;"></div>
        <div id="body" style="flex: 1;"></div>
    </div>
</div>
```

## Performance Considerations

1. **Parse Once, Reuse**: Parse HTML+CSS once during initialization, not every frame
2. **CSS Optimization**: Use external CSS for large stylesheets instead of inline styles
3. **Element IDs**: Keep ID names short to reduce memory usage
4. **Nested Structure**: Avoid deeply nested elements (more than 10 levels) for better performance

## Limitations & Future Improvements

Current limitations:
- Media queries not fully supported
- Animations limited to basic transforms
- Some CSS3 features not implemented
- CSS specificity handled simply (class > tag > inline)

Planned improvements:
- Full CSS3 support
- Animation keyframes
- Media queries for responsive design
- CSS variables/custom properties
- Better performance optimization

## Troubleshooting

### "Parse error: Empty HTML"
**Cause**: The HTML string is empty or contains only whitespace  
**Solution**: Ensure you're passing valid HTML content

### "Error: Duplicate ID found"
**Cause**: Two or more elements have the same ID  
**Solution**: Make sure all element IDs are unique

### "Styles not applying"
**Cause**: CSS selector doesn't match the HTML element  
**Solution**: Check class names and element selectors match exactly

### "Layout not as expected"
**Cause**: Missing width/height or incorrect flex properties  
**Solution**: Review the CSS and ensure all layout properties are set

## Examples

See [Demo/UI/example.html](../Demo/UI/example.html) for complete working examples.

## API Reference

### HtmlCssParser::Parse()

```cpp
static bool Parse(
    const std::string& html,
    const std::string& css,
    std::unique_ptr<UIScreen>& out_screen,
    HTMLParseError& out_error
);
```

**Parameters:**
- `html`: HTML content as string
- `css`: CSS stylesheet as string (can be empty)
- `out_screen`: Output UIScreen object
- `out_error`: Error details if parsing fails

**Returns:** `true` if parsing succeeded, `false` otherwise

### HTMLParseError Structure

```cpp
struct HTMLParseError {
    std::string message;  // Human-readable error message
    int line;            // Line number where error occurred
    int column;          // Column number where error occurred
    std::string context; // Surrounding code context
};
```

## Contributing

To contribute improvements to the HTML+CSS parser:
1. Check existing issues and PRs
2. Create a test case for new features
3. Ensure all tests pass
4. Submit a PR with clear description

---

**Last Updated**: 2024-11  
**Version**: 1.0.0  
**Status**: Stable
