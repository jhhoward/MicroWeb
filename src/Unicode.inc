struct UnicodePage
{
	const char* replacement[128];
};

UnicodePage Latin1Supplement = 
{
	{
		"",		// U+0080		Padding Character	PAD
		"",		// U+0081		High Octet Preset	HOP
		"",		// U+0082		Break Permitted Here	BPH
		"",		// U+0083		No Break Here	NBH
		"",		// U+0084		Index	IND
		"\n",	// U+0085		Next Line	NEL
		"",		// U+0086		Start of Selected Area	SSA
		"",		// U+0087		End of Selected Area	ESA
		"",		// U+0088		Character (Horizontal) Tabulation Set	HTS
		"",		// U+0089		Character (Horizontal) Tabulation with Justification	HTJ
		"",		// U+008A		Line (Vertical) Tabulation Set	LTS
		"",		// U+008B		Partial Line Forward (Down)	PLD
		"",		// U+008C		Partial Line Backward (Up)	PLU
		"",		// U+008D		Reverse Line Feed (Index)	RI
		"",		// U+008E		Single-Shift Two	SS2
		"",		// U+008F		Single-Shift Three	SS3
		"",		// U+0090		Device Control String	DCS
		"",		// U+0091		Private Use One	PU1
		"",		// U+0092		Private Use Two	PU2
		"",		// U+0093		Set Transmit State	STS
		"",		// U+0094		Cancel character	CCH
		"",		// U+0095		Message Waiting	MW
		"",		// U+0096		Start of Protected Area	SPA
		"",		// U+0097		End of Protected Area	EPA
		"",		// U+0098		Start of String	SOS
		"",		// U+0099		Single Graphic Character Introducer	SGCI
		"",		// U+009A		Single Character Introducer	SCI
		"",		// U+009B		Control Sequence Introducer	CSI
		"",		// U+009C		String Terminator	ST
		"",		// U+009D		Operating System Command	OSC
		"",		// U+009E		Private Message	PM
		"",		// U+009F		Application Program Command	APC
		"",		// Latin-1 Punctuation and Symbols
		" ",	// U+00A0	 	Non-breaking space	NBSP
		"",		// U+00A1	¡	Inverted exclamation mark	
		"c",	// U+00A2	¢	Cent sign	
		"£",	// U+00A3	£	Pound sign	
		"",		// U+00A4	¤	Currency sign	
		"Y",	// U+00A5	¥	Yen sign	
		"|",	// U+00A6	¦	Broken bar	
		"",		// U+00A7	§	Section sign	
		"\"",	// U+00A8	¨	Diaeresis	
		"(C)",	// U+00A9	©	Copyright sign	
		"",		// U+00AA	ª	Feminine Ordinal Indicator	
		"<<",	// U+00AB	«	Left-pointing double angle quotation mark	
		"",		// U+00AC	¬	Not sign	
		"",		// U+00AD		Soft hyphen	SHY
		"(R)",	// U+00AE	®	Registered sign	
		"'",	// U+00AF	¯	Macron	
		"",		// U+00B0	°	Degree symbol	
		"+-",	// U+00B1	±	Plus-minus sign	
		"",		// U+00B2	²	Superscript two	
		"",		// U+00B3	³	Superscript three	
		"",		// U+00B4	´	Acute accent	
		"",		// U+00B5	µ	Micro sign	
		"",		// U+00B6	¶	Pilcrow sign	
		"",		// U+00B7	·	Middle dot	
		"."		// U+00B8	¸	Cedilla	
		"",		// U+00B9	¹	Superscript one	
		"",		// U+00BA	º	Masculine ordinal indicator	
		">>",	// U+00BB	»	Right-pointing double-angle quotation mark	
		"1/4",	// U+00BC	¼	Vulgar fraction one quarter	
		"1/2",	// U+00BD	½	Vulgar fraction one half	
		"3/4",	// U+00BE	¾	Vulgar fraction three quarters	
		"",		// U+00BF	¿	Inverted question mark	
		// Letters
		"A",		// U+00C0	À	Latin Capital Letter A with grave	
		"A",		// U+00C1	Á	Latin Capital letter A with acute	
		"A",		// U+00C2	Â	Latin Capital letter A with circumflex	
		"A",		// U+00C3	Ã	Latin Capital letter A with tilde	
		"A",		// U+00C4	Ä	Latin Capital letter A with diaeresis	
		"A",		// U+00C5	Å	Latin Capital letter A with ring above	
		"AE",		// U+00C6	Æ	Latin Capital letter AE	
		"C",		// U+00C7	Ç	Latin Capital letter C with cedilla	
		"E",		// U+00C8	È	Latin Capital letter E with grave	
		"E",		// U+00C9	É	Latin Capital letter E with acute	
		"E",		// U+00CA	Ê	Latin Capital letter E with circumflex	
		"E",		// U+00CB	Ë	Latin Capital letter E with diaeresis	
		"I",		// U+00CC	Ì	Latin Capital letter I with grave	
		"I",		// U+00CD	Í	Latin Capital letter I with acute	
		"I",		// U+00CE	Î	Latin Capital letter I with circumflex	
		"I",		// U+00CF	Ï	Latin Capital letter I with diaeresis	
		"D",		// U+00D0	Ð	Latin Capital letter Eth	
		"N",		// U+00D1	Ñ	Latin Capital letter N with tilde	
		"O",		// U+00D2	Ò	Latin Capital letter O with grave	
		"O",		// U+00D3	Ó	Latin Capital letter O with acute	
		"O",		// U+00D4	Ô	Latin Capital letter O with circumflex	
		"O",		// U+00D5	Õ	Latin Capital letter O with tilde	
		"O",		// U+00D6	Ö	Latin Capital letter O with diaeresis	
		// Mathematical operator
		"x",		// U+00D7	×	Multiplication sign	
		// Letters
		"O",		// U+00D8	Ø	Latin Capital letter O with stroke	
		"U",		// U+00D9	Ù	Latin Capital letter U with grave	
		"U",		// U+00DA	Ú	Latin Capital letter U with acute	
		"U",		// U+00DB	Û	Latin Capital Letter U with circumflex	
		"U",		// U+00DC	Ü	Latin Capital Letter U with diaeresis	
		"Y",		// U+00DD	Ý	Latin Capital Letter Y with acute	
		"",		// U+00DE	Þ	Latin Capital Letter Thorn	
		"",		// U+00DF	ß	Latin Small Letter sharp S	
		"a",		// U+00E0	à	Latin Small Letter A with grave	
		"a",		// U+00E1	á	Latin Small Letter A with acute	
		"a",		// U+00E2	â	Latin Small Letter A with circumflex	
		"a",		// U+00E3	ã	Latin Small Letter A with tilde	
		"a",		// U+00E4	ä	Latin Small Letter A with diaeresis	
		"a",		// U+00E5	å	Latin Small Letter A with ring above	
		"ae",		// U+00E6	æ	Latin Small Letter AE	
		"c",		// U+00E7	ç	Latin Small Letter C with cedilla	
		"e",		// U+00E8	è	Latin Small Letter E with grave	
		"e",		// U+00E9	é	Latin Small Letter E with acute	
		"e",		// U+00EA	ê	Latin Small Letter E with circumflex	
		"e",		// U+00EB	ë	Latin Small Letter E with diaeresis	
		"i",		// U+00EC	ì	Latin Small Letter I with grave	
		"i",		// U+00ED	í	Latin Small Letter I with acute	
		"i",		// U+00EE	î	Latin Small Letter I with circumflex	
		"i",		// U+00EF	ï	Latin Small Letter I with diaeresis	
		"o",		// U+00F0	ð	Latin Small Letter Eth	
		"n",		// U+00F1	ñ	Latin Small Letter N with tilde	
		"o",		// U+00F2	ò	Latin Small Letter O with grave	
		"o",		// U+00F3	ó	Latin Small Letter O with acute	
		"o",		// U+00F4	ô	Latin Small Letter O with circumflex	
		"o",		// U+00F5	õ	Latin Small Letter O with tilde	
		"o",		// U+00F6	ö	Latin Small Letter O with diaeresis	
		// Mathematical operator
		"'/.",		// U+00F7	÷	Division sign	
		// Letters
		"o",		// U+00F8	ø	Latin Small Letter O with stroke	
		"u",		// U+00F9	ù	Latin Small Letter U with grave	
		"u",		// U+00FA	ú	Latin Small Letter U with acute	
		"u",		// U+00FB	û	Latin Small Letter U with circumflex	
		"u",		// U+00FC	ü	Latin Small Letter U with diaeresis	
		"y",		// U+00FD	ý	Latin Small Letter Y with acute	
		"",		// U+00FE	þ	Latin Small Letter Thorn	
		"y",		// U+00FF	ÿ	Latin Small Letter Y with diaeresis
	}
};

UnicodePage LatinExtendedA =
{
	{
		// European Latin
		"A",				// U+0100	Ā	Latin Capital letter A with macron
		"a",				// U+0101	ā	Latin Small letter A with macron
		"A",				// U+0102	Ă	Latin Capital letter A with breve
		"a",				// U+0103	ă	Latin Small letter A with breve
		"A",				// U+0104	Ą	Latin Capital letter A with ogonek
		"a",				// U+0105	ą	Latin Small letter A with ogonek
		"C",				// U+0106	Ć	Latin Capital letter C with acute
		"c",				// U+0107	ć	Latin Small letter C with acute
		"C",				// U+0108	Ĉ	Latin Capital letter C with circumflex
		"c",				// U+0109	ĉ	Latin Small letter C with circumflex
		"C",				// U+010A	Ċ	Latin Capital letter C with dot above
		"c",				// U+010B	ċ	Latin Small letter C with dot above
		"C",				// U+010C	Č	Latin Capital letter C with caron
		"c",				// U+010D	č	Latin Small letter C with caron
		"D",				// U+010E	Ď	Latin Capital letter D with caron
		"d",				// U+010F	ď	Latin Small letter D with caron
		"D",				// U+0110	Đ	Latin Capital letter D with stroke
		"d",				// U+0111	đ	Latin Small letter D with stroke
		"E",				// U+0112	Ē	Latin Capital letter E with macron
		"e",				// U+0113	ē	Latin Small letter E with macron
		"E",				// U+0114	Ĕ	Latin Capital letter E with breve
		"e",				// U+0115	ĕ	Latin Small letter E with breve
		"E",				// U+0116	Ė	Latin Capital letter E with dot above
		"e",				// U+0117	ė	Latin Small letter E with dot above
		"E",				// U+0118	Ę	Latin Capital letter E with ogonek
		"e",				// U+0119	ę	Latin Small letter E with ogonek
		"E",				// U+011A	Ě	Latin Capital letter E with caron
		"e",				// U+011B	ě	Latin Small letter E with caron
		"G",				// U+011C	Ĝ	Latin Capital letter G with circumflex
		"g",				// U+011D	ĝ	Latin Small letter G with circumflex
		"G",				// U+011E	Ğ	Latin Capital letter G with breve
		"g",				// U+011F	ğ	Latin Small letter G with breve
		"G",				// U+0120	Ġ	Latin Capital letter G with dot above
		"g",				// U+0121	ġ	Latin Small letter G with dot above
		"G",				// U+0122	Ģ	Latin Capital letter G with cedilla
		"g",				// U+0123	ģ	Latin Small letter G with cedilla
		"H",				// U+0124	Ĥ	Latin Capital letter H with circumflex
		"h",				// U+0125	ĥ	Latin Small letter H with circumflex
		"H",				// U+0126	Ħ	Latin Capital letter H with stroke
		"h",				// U+0127	ħ	Latin Small letter H with stroke
		"I",				// U+0128	Ĩ	Latin Capital letter I with tilde
		"i",				// U+0129	ĩ	Latin Small letter I with tilde
		"I",				// U+012A	Ī	Latin Capital letter I with macron
		"i",				// U+012B	ī	Latin Small letter I with macron
		"I",				// U+012C	Ĭ	Latin Capital letter I with breve
		"i",				// U+012D	ĭ	Latin Small letter I with breve
		"I",				// U+012E	Į	Latin Capital letter I with ogonek
		"i",				// U+012F	į	Latin Small letter I with ogonek
		"I",				// U+0130	İ	Latin Capital letter I with dot above
		"i",				// U+0131	ı	Latin Small letter dotless I
		"IJ",				// U+0132	Ĳ	Latin Capital Ligature IJ
		"ij",				// U+0133	ĳ	Latin Small Ligature IJ
		"J",				// U+0134	Ĵ	Latin Capital letter J with circumflex
		"j",				// U+0135	ĵ	Latin Small letter J with circumflex
		"K",				// U+0136	Ķ	Latin Capital letter K with cedilla
		"k",				// U+0137	ķ	Latin Small letter K with cedilla
		"k",				// U+0138	ĸ	Latin Small letter Kra
		"L",				// U+0139	Ĺ	Latin Capital letter L with acute
		"l",				// U+013A	ĺ	Latin Small letter L with acute
		"L",				// U+013B	Ļ	Latin Capital letter L with cedilla
		"l",				// U+013C	ļ	Latin Small letter L with cedilla
		"L",				// U+013D	Ľ	Latin Capital letter L with caron
		"l",				// U+013E	ľ	Latin Small letter L with caron
		"L",				// U+013F	Ŀ	Latin Capital letter L with middle dot
		"l",				// U+0140	ŀ	Latin Small letter L with middle dot
		"L",				// U+0141	Ł	Latin Capital letter L with stroke
		"l",				// U+0142	ł	Latin Small letter L with stroke
		"N",				// U+0143	Ń	Latin Capital letter N with acute
		"n",				// U+0144	ń	Latin Small letter N with acute
		"N",				// U+0145	Ņ	Latin Capital letter N with cedilla
		"n",				// U+0146	ņ	Latin Small letter N with cedilla
		"N",				// U+0147	Ň	Latin Capital letter N with caron
		"n",				// U+0148	ň	Latin Small letter N with caron
		"'n",				// U+0149	ŉ	Latin Small letter N preceded by apostrophe
		// European Latin
		"N",				// U+014A	Ŋ	Latin Capital letter Eng
		"n",				// U+014B	ŋ	Latin Small letter Eng
		"O",				// U+014C	Ō	Latin Capital letter O with macron
		"o",				// U+014D	ō	Latin Small letter O with macron
		"O",				// U+014E	Ŏ	Latin Capital letter O with breve
		"o",				// U+014F	ŏ	Latin Small letter O with breve
		"O",				// U+0150	Ő	Latin Capital Letter O with double acute
		"o",				// U+0151	ő	Latin Small Letter O with double acute
		"OE",				// U+0152	Œ	Latin Capital Ligature OE
		"oe",				// U+0153	œ	Latin Small Ligature OE
		"R",				// U+0154	Ŕ	Latin Capital letter R with acute
		"r",				// U+0155	ŕ	Latin Small letter R with acute
		"R",				// U+0156	Ŗ	Latin Capital letter R with cedilla
		"r",				// U+0157	ŗ	Latin Small letter R with cedilla
		"R",				// U+0158	Ř	Latin Capital letter R with caron
		"r",				// U+0159	ř	Latin Small letter R with caron
		"S",				// U+015A	Ś	Latin Capital letter S with acute
		"s",				// U+015B	ś	Latin Small letter S with acute
		"S",				// U+015C	Ŝ	Latin Capital letter S with circumflex
		"s",				// U+015D	ŝ	Latin Small letter S with circumflex
		"S",				// U+015E	Ş	Latin Capital letter S with cedilla
		"s",				// U+015F	ş	Latin Small letter S with cedilla
		"S",				// U+0160	Š	Latin Capital letter S with caron
		"s",				// U+0161	š	Latin Small letter S with caron
		"T",				// U+0162	Ţ	Latin Capital letter T with cedilla
		"t",				// U+0163	ţ	Latin Small letter T with cedilla
		"T",				// U+0164	Ť	Latin Capital letter T with caron
		"t",				// U+0165	ť	Latin Small letter T with caron
		"T",				// U+0166	Ŧ	Latin Capital letter T with stroke
		"t",				// U+0167	ŧ	Latin Small letter T with stroke
		"U",				// U+0168	Ũ	Latin Capital letter U with tilde
		"u",				// U+0169	ũ	Latin Small letter U with tilde
		"U",				// U+016A	Ū	Latin Capital letter U with macron
		"u",				// U+016B	ū	Latin Small letter U with macron
		"U",				// U+016C	Ŭ	Latin Capital letter U with breve
		"u",				// U+016D	ŭ	Latin Small letter U with breve
		"U",				// U+016E	Ů	Latin Capital letter U with ring above
		"u",				// U+016F	ů	Latin Small letter U with ring above
		"U",				// U+0170	Ű	Latin Capital Letter U with double acute
		"u",				// U+0171	ű	Latin Small Letter U with double acute
		"U",				// U+0172	Ų	Latin Capital letter U with ogonek
		"u",				// U+0173	ų	Latin Small letter U with ogonek
		"W",				// U+0174	Ŵ	Latin Capital letter W with circumflex
		"w",				// U+0175	ŵ	Latin Small letter W with circumflex
		"Y",				// U+0176	Ŷ	Latin Capital letter Y with circumflex
		"y",				// U+0177	ŷ	Latin Small letter Y with circumflex
		"Y",				// U+0178	Ÿ	Latin Capital letter Y with diaeresis
		"Z",				// U+0179	Ź	Latin Capital letter Z with acute
		"z",				// U+017A	ź	Latin Small letter Z with acute
		"Z",				// U+017B	Ż	Latin Capital letter Z with dot above
		"z",				// U+017C	ż	Latin Small letter Z with dot above
		"Z",				// U+017D	Ž	Latin Capital letter Z with caron
		"z",				// U+017E	ž	Latin Small letter Z with caron
		"s",				// U+017F	ſ	Latin Small letter long S
	}
};