#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <regex>

using namespace std;

enum class Token {
	KEYWORD, SYMBOL, IDENTIFIER, INT_CONST, STRING_CONST
};

enum class Keyword {
	CLASS, METHOD, FUNCTION, CONSTRUCTOR,
	INT, BOOLEAN, CHAR, VOID, VAR,
	STATIC, FIELD, LET, DO, IF, ELSE,
	WHILE, RETURN, TRUE, FALSE, NULL_, THIS
};

class JackTokenizer {
private:
	ifstream ifs;
	string word;
	Token tokentype;
	Keyword keyword;

	void SkipSpace() {
		while (isspace(ifs.get()));
		ifs.unget();
	}

	void NextWord() {
		word.clear();
		while (true) {
			SkipSpace();
			
			char c = ifs.get();
			while (isalnum(c) || c == '_') {
				word += c;
				c = ifs.get();
			}
			if (word.size()) {
				ifs.unget();
				break;
			}
			
			if (c == '\"') {
				word = '\"';
				c = ifs.get();
				while (c != '\"') {
					word += c;
					c = ifs.get();
				}
				word += '\"';
				break;
			}

			if (c == '/') {
				c = ifs.get();
				if (c == '/') {
					while (ifs.get() != '\n');
				} else if (c == '*') {
					while (true) {
						if (ifs.get() == '*' && ifs.get() == '/')
							break;
					}
				} else {
					ifs.unget();
					word = '/';
					break;
				}
			} else {
				word = c;
				break;
			}
		}
		SkipSpace();
	}
public:
	JackTokenizer() {}
	JackTokenizer(string filename) {
		ifs.open(filename, ios::in);
		Advance();
	}

	bool HasMoreTokens() { return !ifs.eof(); }

	void Advance() {
		const static vector<pair<Keyword, string> > keywords = {
			{Keyword::CLASS, "class"},
			{Keyword::CONSTRUCTOR, "constructor"},
			{Keyword::FUNCTION, "function"},
			{Keyword::METHOD, "method"},
			{Keyword::FIELD, "field"},
			{Keyword::STATIC, "static"},
			{Keyword::VAR, "var"},
			{Keyword::INT, "int"},
			{Keyword::CHAR, "char"},
			{Keyword::BOOLEAN, "boolean"},
			{Keyword::VOID, "void"},
			{Keyword::TRUE, "true"},
			{Keyword::FALSE, "false"},
			{Keyword::NULL_, "null"},
			{Keyword::THIS, "this"},
			{Keyword::LET, "let"},
			{Keyword::DO, "do"},
			{Keyword::IF, "if"},
			{Keyword::ELSE, "else"},
			{Keyword::WHILE, "while"},
			{Keyword::RETURN, "return"}
		};

		NextWord();
		for (auto [key, str] : keywords) {
			if (word == str) {
				tokentype = Token::KEYWORD;
				keyword = key;
				return;
			}
		}
		if (word.size() == 1 && !(isalnum(word[0]) || word[0] == '_')) {
			tokentype = Token::SYMBOL;
			return;
		}
		if (all_of(word.cbegin(), word.cend(),
			[](char c) { return isdigit(c); })) {
			tokentype = Token::INT_CONST;
			return;
		}
		if (word[0] == '\"') {
			tokentype = Token::STRING_CONST;
			return;
		}
		tokentype = Token::IDENTIFIER;
	}

	Token TokenType() { return tokentype; }

	Keyword KeyWord() { return keyword; }

	string Symbol() {
		if (word == "<") return "&lt;";
		if (word == ">") return "&gt;";
		if (word == "&") return "&amp;";
		return word;
	}

	string Identifier() { return word; }

	int IntVal() { return stoi(word); }

	string StringVal() { return word.substr(1, word.size() - 2); }
};

class CompilationEngine {
private:
	ofstream ofs;
	JackTokenizer jt;
	string indent;

	void AddIndent() { indent += "  "; }

	void DeleteIndent() {
		indent.pop_back();
		indent.pop_back();
	}

	void Write(string word) { ofs << indent << word << endl; }

	void WriteKeyword() {
		const static vector<pair<Keyword, string> > keywords = {
			{Keyword::CLASS, "class"},
			{Keyword::CONSTRUCTOR, "constructor"},
			{Keyword::FUNCTION, "function"},
			{Keyword::METHOD, "method"},
			{Keyword::FIELD, "field"},
			{Keyword::STATIC, "static"},
			{Keyword::VAR, "var"},
			{Keyword::INT, "int"},
			{Keyword::CHAR, "char"},
			{Keyword::BOOLEAN, "boolean"},
			{Keyword::VOID, "void"},
			{Keyword::TRUE, "true"},
			{Keyword::FALSE, "false"},
			{Keyword::NULL_, "null"},
			{Keyword::THIS, "this"},
			{Keyword::LET, "let"},
			{Keyword::DO, "do"},
			{Keyword::IF, "if"},
			{Keyword::ELSE, "else"},
			{Keyword::WHILE, "while"},
			{Keyword::RETURN, "return"}
		};

		for (auto [key, str] : keywords) {
			if (jt.KeyWord() == key) {
				Write("<keyword> " + str + " </keyword>");
				jt.Advance();
				return;
			}
		}
	}

	void WriteSymbol() {
		Write("<symbol> " + jt.Symbol() +" </symbol>");
		jt.Advance();
	}

	void WriteIntegerConstant() {
		Write("<integerConstant> " + to_string(jt.IntVal()) + " </integerConstant>");
		jt.Advance();
	}

	void WriteStringConstant() {
		Write("<stringConstant> " + jt.StringVal() + " </stringConstant>");
		jt.Advance();
	}

	void WriteIdentifier() {
		Write("<identifier> " + jt.Identifier() + " </identifier>");
		jt.Advance();
	}

	void WriteType() {
		if (jt.TokenType() == Token::KEYWORD) WriteKeyword();
		else WriteIdentifier();
	}

public:
	CompilationEngine(string ifilename, string ofilename) {
		jt = JackTokenizer(ifilename);
		ofs.open(ofilename);
		CompileClass();
	}

	void CompileClass() {
		Write("<class>");
		AddIndent();

		WriteKeyword();
		WriteIdentifier();
		WriteSymbol();
		while (jt.TokenType() == Token::KEYWORD &&
			(jt.KeyWord() == Keyword::STATIC || jt.KeyWord() == Keyword::FIELD)) {
			CompileClassVarDec();
		}
		while (jt.TokenType() == Token::KEYWORD &&
			(jt.KeyWord() == Keyword::CONSTRUCTOR || jt.KeyWord() == Keyword::FUNCTION ||
				jt.KeyWord() == Keyword::METHOD)) {
			CompileSubroutine();
		}
		WriteSymbol();

		DeleteIndent();
		Write("</class>");
	}

	void CompileClassVarDec() {
		Write("<classVarDec>");
		AddIndent();

		WriteKeyword();
		WriteType();
		WriteIdentifier();
		while (jt.Symbol() != ";") {
			WriteSymbol();
			WriteIdentifier();
		}
		WriteSymbol();

		DeleteIndent();
		Write("</classVarDec>");
	}

	void CompileSubroutine() {
		Write("<subroutineDec>");
		AddIndent();

		WriteKeyword();
		if (jt.TokenType() == Token::KEYWORD) WriteKeyword();
		else WriteType();
		WriteIdentifier();
		WriteSymbol();
		CompileParameterList();
		WriteSymbol();

		Write("<subroutineBody>");
		AddIndent();

		WriteSymbol();
		while (jt.KeyWord() == Keyword::VAR)
			CompileVarDec();
		CompileStatements();
		WriteSymbol();

		DeleteIndent();
		Write("</subroutineBody>");

		DeleteIndent();
		Write("</subroutineDec>");
	}

	void CompileParameterList() {
		Write("<parameterList>");
		AddIndent();

		if (jt.TokenType() != Token::SYMBOL) {
			WriteType();
			WriteIdentifier();
			while (jt.Symbol() == ",") {
				WriteSymbol();
				WriteType();
				WriteIdentifier();
			}
		}

		DeleteIndent();
		Write("</parameterList>");
	}

	void CompileVarDec() {
		Write("<varDec>");
		AddIndent();

		WriteKeyword();
		WriteType();
		WriteIdentifier();
		while (jt.Symbol() == ",") {
			WriteSymbol();
			WriteIdentifier();
		}
		WriteSymbol();

		DeleteIndent();
		Write("</varDec>");
	}

	void CompileStatements() {
		Write("<statements>");
		AddIndent();

		while (jt.TokenType() == Token::KEYWORD) {
			if (jt.KeyWord() == Keyword::LET) CompileLet();
			else if (jt.KeyWord() == Keyword::IF) CompileIf();
			else if (jt.KeyWord() == Keyword::WHILE) CompileWhile();
			else if (jt.KeyWord() == Keyword::DO) CompileDo();
			else if (jt.KeyWord() == Keyword::RETURN) CompileReturn();
			else
				break;
		}

		DeleteIndent();
		Write("</statements>");
	}

	void CompileDo() {
		Write("<doStatement>");
		AddIndent();

		WriteKeyword();
		WriteIdentifier();
		if (jt.Symbol() == ".") {
			WriteSymbol();
			WriteIdentifier();
		}
		WriteSymbol();
		CompileExpressionList();
		WriteSymbol();
		WriteSymbol();

		DeleteIndent();
		Write("</doStatement>");
	}

	void CompileLet() {
		Write("<letStatement>");
		AddIndent();

		WriteKeyword();
		WriteIdentifier();
		if (jt.Symbol() == "[") {
			WriteSymbol();
			CompileExpression();
			WriteSymbol();
		}
		WriteSymbol();
		CompileExpression();
		WriteSymbol();

		DeleteIndent();
		Write("</letStatement>");
	}

	void CompileWhile() {
		Write("<whileStatement>");
		AddIndent();

		WriteKeyword();
		WriteSymbol();
		CompileExpression();
		WriteSymbol();
		WriteSymbol();
		CompileStatements();
		WriteSymbol();

		DeleteIndent();
		Write("</whileStatement>");
	}

	void CompileReturn() {
		Write("<returnStatement>");
		AddIndent();

		WriteKeyword();
		if (jt.TokenType() != Token::SYMBOL || jt.Symbol() != ";")
			CompileExpression();
		WriteSymbol();

		DeleteIndent();
		Write("</returnStatement>");
	}

	void CompileIf() {
		Write("<ifStatement>");
		AddIndent();

		WriteKeyword();
		WriteSymbol();
		CompileExpression();
		WriteSymbol();
		WriteSymbol();
		CompileStatements();
		WriteSymbol();
		if (jt.TokenType() == Token::KEYWORD && jt.KeyWord() == Keyword::ELSE) {
			WriteKeyword();
			WriteSymbol();
			CompileStatements();
			WriteSymbol();
		}

		DeleteIndent();
		Write("</ifStatement>");
	}

	void CompileExpression() {
		auto isOp = [](string x) {
			for (auto& s : { "+","-","*","/","&amp;","|","&lt;","&gt;","=" })
				if (x == s) return true;
			return false;
		};

		Write("<expression>");
		AddIndent();

		CompileTerm();
		while (jt.TokenType() == Token::SYMBOL && isOp(jt.Symbol())) {
			WriteSymbol();
			CompileTerm();
		}

		DeleteIndent();
		Write("</expression>");
	}

	void CompileTerm() {
		Write("<term>");
		AddIndent();

		if (jt.TokenType() == Token::INT_CONST) {
			WriteIntegerConstant();
		} else if (jt.TokenType() == Token::STRING_CONST) {
			WriteStringConstant();
		} else if (jt.TokenType() == Token::KEYWORD) {
			WriteKeyword();
		} else if (jt.TokenType() == Token::IDENTIFIER) {
			WriteIdentifier();
			if (jt.TokenType() == Token::SYMBOL) {
				if (jt.Symbol() == "[") {
					WriteSymbol();
					CompileExpression();
					WriteSymbol();
				} else if (jt.Symbol() == "(" || jt.Symbol() == ".") {
					if (jt.Symbol() == ".") {
						WriteSymbol();
						WriteIdentifier();
					}
					WriteSymbol();
					CompileExpressionList();
					WriteSymbol();
				}
			}
		} else {
			if (jt.Symbol() == "(") {
				WriteSymbol();
				CompileExpression();
				WriteSymbol();
			} else {
				WriteSymbol();
				CompileTerm();
			}
		}

		DeleteIndent();
		Write("</term>");
	}

	void CompileExpressionList() {
		Write("<expressionList>");
		AddIndent();

		if (jt.TokenType() != Token::SYMBOL || jt.Symbol() != ")") {
			CompileExpression();
			while (jt.TokenType() == Token::SYMBOL && jt.Symbol() == ",") {
				WriteSymbol();
				CompileExpression();
			}
		}

		DeleteIndent();
		Write("</expressionList>");
	}
};

class JackAnalyzer {
public:
	JackAnalyzer(string source) {
		vector<string> files;

		namespace fs = filesystem;
		if (fs::is_directory(source)) {
			for (auto& p : fs::directory_iterator(source))
				if (p.path().extension() == ".jack")
					files.push_back(p.path().string());
		} else {
			files.push_back(source);
		}

		for (auto& file : files) {
			CompilationEngine(file, file.substr(0, file.size() - 5) + ".compile.xml");
		}
	}
};

int main(int argc, char** argv) {
	string source = argv[1];
	JackAnalyzer ja(source);

	return 0;
}