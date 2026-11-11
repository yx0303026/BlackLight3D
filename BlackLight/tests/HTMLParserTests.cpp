#include "rmlui/HTMLParser.h"
#include "rmlui/HTMLElement.h"

#include <cassert>
#include <iostream>
#include <sstream>

using namespace BlackLight::UI;

// Test helper
void AssertEqual(bool condition, const std::string& message) {
	if (!condition) {
		std::cerr << "❌ FAILED: " << message << std::endl;
		std::exit(1);
	}
	std::cout << "✅ PASSED: " << message << std::endl;
}

// Test 1: Parse simple HTML structure
void TestSimpleHTML() {
	std::string html = R"(
		<div id="container" class="main-panel">
			<h1>Title</h1>
			<p>Content here</p>
		</div>
	)";

	std::unique_ptr<HTMLDocument> doc;
	HTMLParseError error;

	bool result = HTMLParser::Parse(html, "", doc, error);
	AssertEqual(result, "Simple HTML parsing");
	AssertEqual(doc != nullptr, "Document created");
	AssertEqual(doc->GetChildren().size() > 0, "Document has children");
}

// Test 2: Parse HTML with buttons
void TestButtonHTML() {
	std::string html = R"(
		<div id="form">
			<button id="submit-btn" class="btn btn-primary">Submit</button>
			<button id="cancel-btn" class="btn btn-secondary">Cancel</button>
		</div>
	)";

	std::unique_ptr<HTMLDocument> doc;
	HTMLParseError error;

	bool result = HTMLParser::Parse(html, "", doc, error);
	AssertEqual(result, "Button HTML parsing");

	auto* submit = doc->FindById("submit-btn");
	AssertEqual(submit != nullptr, "Find button by ID");
}

// Test 3: Parse HTML with forms
void TestFormHTML() {
	std::string html = R"(
		<form id="user-form">
			<input type="text" id="username" placeholder="Username"/>
			<input type="password" id="password" placeholder="Password"/>
			<textarea id="bio">Bio text</textarea>
			<select id="role">
				<option>Admin</option>
				<option>User</option>
			</select>
		</form>
	)";

	std::unique_ptr<HTMLDocument> doc;
	HTMLParseError error;

	bool result = HTMLParser::Parse(html, "", doc, error);
	AssertEqual(result, "Form HTML parsing");
}

// Test 4: Parse HTML with lists
void TestListHTML() {
	std::string html = R"(
		<ul id="menu">
			<li>Home</li>
			<li>About</li>
			<li>Contact</li>
		</ul>
	)";

	std::unique_ptr<HTMLDocument> doc;
	HTMLParseError error;

	bool result = HTMLParser::Parse(html, "", doc, error);
	AssertEqual(result, "List HTML parsing");
}

// Test 5: Parse HTML with CSS stylesheet
void TestHTMLWithCSS() {
	std::string html = "<div id='app'><button>Click Me</button></div>";
	std::string css = R"(
		div { display: flex; }
		button { color: white; background: blue; }
	)";

	std::unique_ptr<HTMLDocument> doc;
	HTMLParseError error;

	bool result = HTMLParser::ParseWithCss(html, css, doc, error);
	AssertEqual(result, "HTML+CSS parsing");
	AssertEqual(doc->GetStyleSheets().size() > 0, "Stylesheet added");
}

// Test 6: Validate HTML
void TestHTMLValidation() {
	std::string valid_html = "<div><p>Valid</p></div>";
	std::string invalid_html = "<div><p>Invalid</div></p>";

	HTMLParseError error;

	bool result1 = HTMLParser::Validate(valid_html, error);
	AssertEqual(result1, "Valid HTML validation");
}

// Test 7: Element attribute parsing
void TestAttributeParsing() {
	std::string html = R"(
		<input id="email" class="form-input" style="padding: 8px" type="email"/>
	)";

	std::unique_ptr<HTMLDocument> doc;
	HTMLParseError error;

	HTMLParser::Parse(html, "", doc, error);

	auto* input = doc->FindById("email");
	AssertEqual(input != nullptr, "Element found by ID");
	AssertEqual(input->GetClasses().size() > 0, "CSS classes parsed");
	AssertEqual(!input->GetStyle().empty(), "Inline styles parsed");
}

// Test 8: Element search by class
void TestFindByClass() {
	std::string html = R"(
		<div>
			<button class="btn btn-primary">Button 1</button>
			<button class="btn btn-secondary">Button 2</button>
			<a class="btn btn-link">Link</a>
		</div>
	)";

	std::unique_ptr<HTMLDocument> doc;
	HTMLParseError error;

	HTMLParser::Parse(html, "", doc, error);

	auto buttons = doc->FindByClass("btn");
	AssertEqual(buttons.size() >= 2, "Find elements by class");
}

// Test 9: Nested containers
void TestNestedContainers() {
	std::string html = R"(
		<div id="outer" class="container">
			<div id="middle" class="panel">
				<div id="inner" class="card">
					<p>Nested content</p>
				</div>
			</div>
		</div>
	)";

	std::unique_ptr<HTMLDocument> doc;
	HTMLParseError error;

	HTMLParser::Parse(html, "", doc, error);

	auto* inner = doc->FindById("inner");
	AssertEqual(inner != nullptr, "Find nested element");
	AssertEqual(inner->GetParent() != nullptr, "Element has parent");
}

// Test 10: Complex UI structure
void TestComplexUI() {
	std::string html = R"(
		<div id="app" class="main">
			<header id="header" class="header">
				<h1>My App</h1>
			</header>
			<main id="content" class="content">
				<nav id="sidebar" class="sidebar">
					<ul>
						<li><a href="#home">Home</a></li>
						<li><a href="#about">About</a></li>
					</ul>
				</nav>
				<section id="main-content" class="section">
					<h2>Welcome</h2>
					<form id="settings">
						<input id="setting1" type="text"/>
						<button id="save-btn" class="btn">Save</button>
					</form>
				</section>
			</main>
			<footer id="footer" class="footer">
				<p>&copy; 2024</p>
			</footer>
		</div>
	)";

	std::unique_ptr<HTMLDocument> doc;
	HTMLParseError error;

	bool result = HTMLParser::Parse(html, "", doc, error);
	AssertEqual(result, "Complex UI structure parsing");
	AssertEqual(doc->FindById("app") != nullptr, "Root app found");
	AssertEqual(doc->FindById("header") != nullptr, "Header found");
	AssertEqual(doc->FindById("sidebar") != nullptr, "Sidebar found");
	AssertEqual(doc->FindById("save-btn") != nullptr, "Button found");
}

int main() {
	std::cout << "\n🧪 Running HTML Parser Tests...\n" << std::endl;

	TestSimpleHTML();
	TestButtonHTML();
	TestFormHTML();
	TestListHTML();
	TestHTMLWithCSS();
	TestHTMLValidation();
	TestAttributeParsing();
	TestFindByClass();
	TestNestedContainers();
	TestComplexUI();

	std::cout << "\n✨ All tests passed! ✨\n" << std::endl;
	return 0;
}
