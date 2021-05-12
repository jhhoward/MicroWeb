#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

/*
class HTMLNode
{
public:
	enum NodeType
	{
		Root,
		HTML,
		Body,
		GenericTag,
		Text,
	};

	HTMLNode(NodeType inNodeType, HTMLNode* inParentNode);
	
	NodeType type;
	HTMLNode* parentNode;
	HTMLNode* firstChild;
	HTMLNode* nextNode;
	char* content;
	
	void AddChild(HTMLNode* child);
};

HTMLNode::HTMLNode(NodeType inNodeType, HTMLNode* inParentNode) 
: type(inNodeType)
, parentNode(inParentNode)
, firstChild(NULL)
, nextNode(NULL)
, content(NULL)
{
	if(parentNode)
	{
		parentNode->AddChild(this);
	}
}

void HTMLNode::AddChild(HTMLNode* child)
{
	if(firstChild == NULL)
	{
		firstChild = child;
	}
	else
	{
		HTMLNode* node = firstChild; 
		while(firstChild->nextNode)
		{
			node = firstChild->nextNode;
		}
		node->nextNode = child;
	}
}
*/

enum StyleMaskTypes
{
	Style_Bold = 1,
	Style_Italic = 2,
	Style_Underline = 4,
	Style_H1 = 8,
	Style_H2 = 16,
	Style_H3 = 32,
	Style_H4 = 64,
	Style_Center = 128
};

typedef uint16_t StyleMask;

enum HTMLParseSection
{
	Section_Document,
	Section_Head,
	Section_Body,
	Section_Script,
	Section_Style
};

class HTMLTagHandler
{
public:
	HTMLTagHandler(const char* inName) : name(inName) {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const {}
	virtual void Close(class HTMLParser& parser) const {}
	
	const char* name;
};

class SectionTagHandler : public HTMLTagHandler
{
public:
	SectionTagHandler(const char* inName, HTMLParseSection inSection) : HTMLTagHandler(inName), section(inSection) {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
	virtual void Close(class HTMLParser& parser) const;
	const HTMLParseSection section;
};

class BrTagHandler : public HTMLTagHandler
{
public:
	BrTagHandler() : HTMLTagHandler("br") {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
};

class HrTagHandler : public HTMLTagHandler
{
public:
	HrTagHandler() : HTMLTagHandler("hr") {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
};

class HTagHandler : public HTMLTagHandler
{
public:
	HTagHandler(const char* inName, int inSize) : HTMLTagHandler(inName), size(inSize) {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
	virtual void Close(class HTMLParser& parser) const;
	const int size;
};

class StyleTagHandler : public HTMLTagHandler
{
public:
	StyleTagHandler(const char* inName, StyleMask inStyle) : HTMLTagHandler(inName), style(inStyle) {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
	virtual void Close(class HTMLParser& parser) const;
	const StyleMask style;
};

class LiTagHandler : public HTMLTagHandler
{
public:
	LiTagHandler() : HTMLTagHandler("li") {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
	virtual void Close(class HTMLParser& parser) const;
};

class ATagHandler : public HTMLTagHandler
{
public:
	ATagHandler() : HTMLTagHandler("a") {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
	virtual void Close(class HTMLParser& parser) const;
};

class PTagHandler : public HTMLTagHandler
{
public:
	PTagHandler() : HTMLTagHandler("p") {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
	virtual void Close(class HTMLParser& parser) const;
};

class DivTagHandler : public HTMLTagHandler
{
public:
	DivTagHandler() : HTMLTagHandler("div") {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
	virtual void Close(class HTMLParser& parser) const;
};

struct HTMLTag
{
	static const HTMLTagHandler* tagHandlers[];
	static const HTMLTagHandler* DetermineType(const char* str);
};

const HTMLTagHandler* HTMLTag::tagHandlers[] =
{
	new HTMLTagHandler("generic"),
	new SectionTagHandler("html", Section_Document),
	new SectionTagHandler("head", Section_Head),
	new SectionTagHandler("body", Section_Body),
	new SectionTagHandler("script", Section_Script),
	new SectionTagHandler("style", Section_Style),
	new HTagHandler("h1", 1),
	new HTagHandler("h2", 2),
	new HTagHandler("h3", 3),
	new HTagHandler("h4", 4),
	new HTagHandler("h5", 5),
	new HTagHandler("h6", 6),
	new PTagHandler(),
	new BrTagHandler(),
	new StyleTagHandler("b", Style_Bold),
	new StyleTagHandler("i", Style_Italic),
	new StyleTagHandler("u", Style_Underline),
	new ATagHandler(),
	new HTMLTagHandler("img"),
	new LiTagHandler(),
	new HrTagHandler(),
	new DivTagHandler(),
	NULL
};


const HTMLTagHandler* HTMLTag::DetermineType(const char* str)
{
	for(int n = 0; ; n++)
	{
		const HTMLTagHandler* handler = tagHandlers[n];
		if (!handler)
		{
			break;
		}
		const char* strPtr = str;
		const char* tagPtr = handler->name;
		bool match = true;
		
		while(*strPtr && *tagPtr)
		{
			if(tolower(*strPtr) != *tagPtr)
			{
				match = false;
				break;
			}
			strPtr++;
			tagPtr++;
		}
		
		if(match && *strPtr == '\0' && *tagPtr == '\0')
		{
			return handler;
		}
	}
	
	static const HTMLTagHandler genericTag("generic");
	return &genericTag;
}

#define MAX_STYLE_STACK_SIZE 32

class HTMLRenderer
{
public:
	HTMLRenderer() : currentLineLength(0), styleStackSize(0) 
	{
		styleStack[0] = 0;
	}
	void DrawText(const char* str);
	void BreakLine(int margin = 0);
	void EnsureNewLine();
	void IncreaseLineHeight(int lineHeight) {}
	
	void PushStyle(StyleMask style);
	void PopStyle();

private:
	int currentLineLength;
	StyleMask styleStack[MAX_STYLE_STACK_SIZE];
	uint8_t styleStackSize;
};

class AttributeParser
{
public:
	AttributeParser(char* inAttributeString) : attributeString(inAttributeString), key(NULL), value(NULL) {}
	bool Parse();
	
	const char* Key() { return key; }
	const char* Value() { return value; }
	
private:
	bool IsWhiteSpace(char c);
	
	char* attributeString;
	char* key;
	char* value;
};

void HTMLRenderer::DrawText(const char* str)
{
	StyleMask style = styleStack[styleStackSize];

	while(*str)
	{
		int wordLength = 0;
		const char* wordPtr = str;
		while(*wordPtr && *wordPtr != ' ')
		{
			wordPtr++;
			wordLength++;
		}
		if(currentLineLength + wordLength >= 80)
		{
			putchar('\n');
			currentLineLength = 0;
		}
		for(int n = 0; n < wordLength; n++)
		{
			if(str[n] > 0)
			{
				if (style & Style_Bold)
				{
					putchar(toupper(str[n]));
				}
				else
				{
					putchar(str[n]);
				}
				currentLineLength++;
			}
		}
		
		str += wordLength;
		if(*str)
		{
			if(*str > 0)
			{
				if (style & Style_Bold)
				{
					putchar(toupper(*str));
				}
				else
				{
					putchar(*str);
				}
				currentLineLength++;
			}
			str++;
		}
	}
}

void HTMLRenderer::BreakLine(int margin)
{
	printf("\n");
	currentLineLength = 0;
}

void HTMLRenderer::EnsureNewLine()
{
	if(currentLineLength > 0)
	{
		printf("\n");
		currentLineLength = 0;
	}
}

void HTMLRenderer::PushStyle(StyleMask style)
{
	if (styleStackSize < MAX_STYLE_STACK_SIZE - 1)
	{
		styleStackSize++;
		styleStack[styleStackSize] = style;
	}
}

void HTMLRenderer::PopStyle()
{
	if (styleStackSize > 0)
	{
		styleStackSize--;
	}
}

#define MAX_PARSE_SECTION_STACK_SIZE 32

class HTMLParser
{
public:
	HTMLParser(HTMLRenderer& renderer);

	void Parse(char* buffer, size_t count);

	HTMLRenderer& renderer;

	void PushSection(HTMLParseSection section);
	void PopSection(HTMLParseSection section);
	HTMLParseSection CurrentSection() { return sectionStack[sectionStackSize]; }

private:
	//HTMLNode* CreateNode(HTMLNode::NodeType nodeType, HTMLNode* parentNode);
	void AppendTextBuffer(char c);
	void FlushTextBuffer();
	bool IsWhiteSpace(char c);

	enum ParseState
	{
		ParseText,
		ParsePossibleTag,
		ParseTag,
		ParseAmpersandEscape
	};
	
	ParseState parseState;
	char textBuffer[256];
	size_t textBufferSize;

	HTMLParseSection sectionStack[MAX_PARSE_SECTION_STACK_SIZE];
	unsigned int sectionStackSize;

	/*
	HTMLNode* documentNode;
	HTMLNode* currentParseNode;
	*/
};

HTMLParser::HTMLParser(HTMLRenderer& inRenderer)
: renderer(inRenderer)
, sectionStackSize(0)
, parseState(ParseText)
, textBufferSize(0)
{
	sectionStack[0] = Section_Document;
	/*
	documentNode = CreateNode(HTMLNode::Root, NULL);
	currentParseNode = documentNode;
	*/
}

void HTMLParser::PushSection(HTMLParseSection section)
{
	if (sectionStackSize < MAX_PARSE_SECTION_STACK_SIZE - 1)
	{
		sectionStackSize++;
		sectionStack[sectionStackSize] = section;
	}
	else
	{
		// TODO: ERROR
	}
}

void HTMLParser::PopSection(HTMLParseSection section)
{
	if (sectionStackSize > 0)
	{
		if (CurrentSection() != section)
		{
			// TODO ERROR
		}

		sectionStackSize--;
	}
	else
	{
		// TODO: ERROR
	}
}

#define NUM_AMPERSAND_ESCAPE_SEQUENCES 14
const char* ampersandEscapeSequences[14 * 2] = 
{
	"quot",	"\"",
	"amp",	"&",
	"lt",	"<",
	"gt",	">",
	"nbsp",	" ",
	"pound",	"£",
	"brvbar",	"¦",
	"uml",	"\"",
	"not", "¬",
	"cent", "c",
	"copy", "(C)",
	"reg", "(R)",
	"laquo", "<<",
	"raquo", ">>",
};

void HTMLParser::AppendTextBuffer(char c)
{
	if(textBufferSize == sizeof(textBuffer) - 1)
	{
		FlushTextBuffer();
	}
	if(textBufferSize < sizeof(textBuffer) - 1)
	{
		textBuffer[textBufferSize] = c;
		textBufferSize++;
	}
}

void HTMLParser::FlushTextBuffer()
{
	textBuffer[textBufferSize] = '\0';
	
	switch(parseState)
	{
		case ParseText:
		{
			if(CurrentSection() == Section_Body  && textBufferSize > 0)
			{
				renderer.DrawText(textBuffer);
			}
		}
		break;
		
		case ParseTag:
		{
			if (textBufferSize > 0)
			{
				char* tagStr = textBuffer;
				bool isCloseTag = false;

				if (tagStr[0] == '/')
				{
					isCloseTag = true;
					tagStr++;
					while (IsWhiteSpace(*tagStr))
					{
						tagStr++;
					}
				}

				char* attributeStr = tagStr;
				while (*attributeStr && !IsWhiteSpace(*attributeStr))
				{
					attributeStr++;
				}
				if (*attributeStr)
				{
					*attributeStr = '\0';
					attributeStr++;
				}

				const HTMLTagHandler* tagHandler = HTMLTag::DetermineType(tagStr);

				if (isCloseTag)
				{
					tagHandler->Close(*this);
				}
				else
				{
					tagHandler->Open(*this, attributeStr);
				}
			}
		}		
		break;
		case ParseAmpersandEscape:
		{
			if(textBufferSize == 0)
			{
				renderer.DrawText("&");
			}
			else
			{
				for(int n = 0; n < NUM_AMPERSAND_ESCAPE_SEQUENCES; n++)
				{
					const char* escapeSequence = ampersandEscapeSequences[n * 2];
					bool matching = true;
					
					for(int i = 0; i < textBufferSize; i++)
					{
						if(escapeSequence[i] == '\0')
						{
							matching = false;
							break;
						}
						if(tolower(textBuffer[i]) != escapeSequence[i])
						{
							matching = false;
							break;
						}
					}
					
					if(matching && escapeSequence[textBufferSize] == '\0')
					{
						renderer.DrawText(ampersandEscapeSequences[n * 2 + 1]);
						break;
					}
				}
			}
		}
		break;
	}
	
	textBufferSize = 0;
	textBuffer[0] = '\0';
}

bool HTMLParser::IsWhiteSpace(char c)
{
	return c == ' ' || c == '\n' || c == '\t';
}

void HTMLParser::Parse(char* buffer, size_t count)
{
	while(count)
	{
		char c = *buffer++;
		
		switch(parseState)
		{
			case ParseText:
			if(c == '<')
			{
				parseState = ParsePossibleTag;
			}
			else if(c == '&')
			{
				FlushTextBuffer();
				parseState = ParseAmpersandEscape;
			}
			else
			{
				if(IsWhiteSpace(c))
				{
					c = ' ';
					if(textBufferSize == 0)
					{
						break;
					}
					else
					{
						if(IsWhiteSpace(textBuffer[textBufferSize - 1]))
						{
							break;
						}
					}
				}

				AppendTextBuffer(c);
			}
			break;
			
			case ParsePossibleTag:
			parseState = ParseText;
			if(IsWhiteSpace(c))
			{
				AppendTextBuffer('<');
				AppendTextBuffer(' ');
			}
			else
			{
				FlushTextBuffer();
				parseState = ParseTag;
				AppendTextBuffer(c);
			}
			break;
			
			case ParseTag:
			if(c == '>')
			{
				FlushTextBuffer();
				parseState = ParseText;
			}
			else
			{
				AppendTextBuffer(c);
			}
			
			break;
			
			case ParseAmpersandEscape:
			if(c == ';' || IsWhiteSpace(c))
			{
				FlushTextBuffer();
				parseState = ParseText;
				if(IsWhiteSpace(c))
				{
					AppendTextBuffer(' ');
				}
			}
			else
			{
				AppendTextBuffer(c);
			}
			break;
		}
		
		count--;
	}
}

void ParseFile(const char* filename)
{
	FILE* fs = fopen(filename, "r");
	
	if(fs)
	{
		HTMLRenderer renderer;
		HTMLParser parser(renderer);
		char buffer[50];
	
		while(!feof(fs))
		{
			size_t bytesRead = fread(buffer, 1, 50, fs);
			if(!bytesRead)
			{
				break;
			}
			parser.Parse(buffer, bytesRead);
		}
	
		fclose(fs);
	}
	else
	{
		fprintf(stderr, "Error opening %s\n", filename);
	}
}

void HrTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	parser.renderer.BreakLine();
	parser.renderer.DrawText("---------------------------");
	parser.renderer.BreakLine();
}

void BrTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	parser.renderer.BreakLine();
}

void HTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	parser.renderer.BreakLine();
	parser.renderer.DrawText("== ");
}
void HTagHandler::Close(class HTMLParser& parser) const
{
	parser.renderer.DrawText(" ==");
	parser.renderer.BreakLine();
}

void LiTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	parser.renderer.EnsureNewLine();
	parser.renderer.DrawText(" * ");
}
void LiTagHandler::Close(class HTMLParser& parser) const
{
	parser.renderer.EnsureNewLine();
}

void ATagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	AttributeParser attributes(attributeStr);
	while(attributes.Parse())
	{
		//printf("KEY: %s = %s\n", attributes.Key(), attributes.Value());
	}
	//printf("{attributes='%s'}\n", attributeStr);
	parser.renderer.DrawText("[");
}
void ATagHandler::Close(class HTMLParser& parser) const
{
	parser.renderer.DrawText("]");
}

void PTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	parser.renderer.BreakLine();
}
void PTagHandler::Close(class HTMLParser& parser) const
{
	parser.renderer.BreakLine();
}

void DivTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	parser.renderer.EnsureNewLine();
}
void DivTagHandler::Close(class HTMLParser& parser) const
{
	parser.renderer.EnsureNewLine();
}

void SectionTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	parser.PushSection(section);
}
void SectionTagHandler::Close(class HTMLParser& parser) const
{
	parser.PopSection(section);
}

void StyleTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	parser.renderer.PushStyle(style);
}
void StyleTagHandler::Close(class HTMLParser& parser) const
{
	parser.renderer.PopStyle();
}

bool AttributeParser::Parse()
{
	key = value = NULL;

	// Skip any white space
	while(*attributeString && IsWhiteSpace(*attributeString))
	{
		attributeString++;
	}
	if(*attributeString == '\0')
	{
		return false;
	}
	
	bool foundEquals = false;
	
	// Parse key
	if(*attributeString == '"')
	{
		attributeString++;
		key = attributeString;
		while(*attributeString != '"')
		{
			if(*attributeString == '\0')
			{
				return false;
			}
			attributeString++;
		}
		*attributeString = '\0';
		attributeString++;
	}
	else
	{
		key = attributeString;
		while(!IsWhiteSpace(*attributeString))
		{
			if(*attributeString == '\0')
			{
				return false;
			}
			if(*attributeString == '=')
			{
				foundEquals = true;
				break;
			}
			attributeString++;
		}
		*attributeString = '\0';
		attributeString++;
	}
	
	// Parse =
	while(!foundEquals)
	{
		if(*attributeString == '\0')
		{
			return false;
		}
		if(*attributeString == '=')
		{
			break;
		}
		if(!IsWhiteSpace(*attributeString))
		{
			return false;
		}
		attributeString++;
	}
	
	// Skip any white space
	while(*attributeString && IsWhiteSpace(*attributeString))
	{
		attributeString++;
	}
	if(*attributeString == '\0')
	{
		return false;
	}
	
	// Parse value
	if(*attributeString == '"')
	{
		attributeString++;
		value = attributeString;
		while(*attributeString != '"')
		{
			if(*attributeString == '\0')
			{
				return false;
			}
			attributeString++;
		}
		*attributeString = '\0';
		attributeString++;
	}
	else
	{
		value = attributeString;
		while(*attributeString && !IsWhiteSpace(*attributeString))
		{
			attributeString++;
		}
		
		if(*attributeString)
		{
			*attributeString = '\0';
			attributeString++;
		}
	}
			
	return true;
}

bool AttributeParser::IsWhiteSpace(char c)
{
	return c == ' ' || c == '\n' || c == '\t';
}

int main(int argc, char* argv[])
{
	if(argc != 2)
	{
		fprintf(stderr, "Usage: %s [file]\n", argv[0]);
		return 0;
	}
	
	ParseFile(argv[1]);
	
	return 0;
}
