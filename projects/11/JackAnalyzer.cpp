#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <map>
#include <utility>
#include <tuple>

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

enum class Kind {
	STATIC, FIELD, ARG, VAR, NONE
};

enum class Segment {
	CONST, ARG, LOCAL, STATIC, THIS, THAT, POINTER, TEMP
};

enum class Command {
	ADD, SUB, NEG, EQ, GT, LT, AND, OR, NOT
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

class SymbolTable {
private:
	int staticcnt, fieldcnt, argcnt, varcnt;
	map<string, tuple<Kind, string, int> > classtable, subroutinetable;
public:
	SymbolTable() {
		staticcnt = fieldcnt = argcnt = varcnt = 0;
	}

	void StartSubroutine() {
		subroutinetable.clear();
		argcnt = varcnt = 0;
	}

	void Define(string name, string type, Kind kind) {
		if (kind == Kind::STATIC) {
			classtable[name] = tuple(kind, type, staticcnt);
			++staticcnt;
		} else if(kind == Kind::FIELD) {
			classtable[name] = tuple(kind, type, fieldcnt);
			++fieldcnt;
		} else if (kind == Kind::ARG) {
			subroutinetable[name] = tuple(kind, type, argcnt);
			++argcnt;
		} else {
			subroutinetable[name] = tuple(kind, type, varcnt);
			++varcnt;
		}
	}

	int VarCount(Kind kind) {
		if (kind == Kind::STATIC) return staticcnt;
		if (kind == Kind::FIELD) return fieldcnt;
		if (kind == Kind::ARG) return argcnt;
		return varcnt;
	}

	Kind KindOf(string name) {
		if (subroutinetable.count(name)) return get<0>(subroutinetable[name]);
		if (classtable.count(name)) return get<0>(classtable[name]);
		return Kind::NONE;
	}

	string TypeOf(string name) {
		if (subroutinetable.count(name)) return get<1>(subroutinetable[name]);
		if (classtable.count(name)) return get<1>(classtable[name]);
	}

	int IndexOf(string name) {
		if (subroutinetable.count(name)) return get<2>(subroutinetable[name]);
		if (classtable.count(name)) return get<2>(classtable[name]);
	}
};

class VMWriter {
private:
	ofstream ofs;

	string Segtostr(Segment segment) {
		if (segment == Segment::CONST) return "constant";
		if (segment == Segment::ARG) return "argument";
		if (segment == Segment::LOCAL) return "local";
		if (segment == Segment::STATIC) return "static";
		if (segment == Segment::THIS) return "this";
		if (segment == Segment::THAT) return "that";
		if (segment == Segment::POINTER) return "pointer";
		return "temp";
	}
public:
	VMWriter() {}
	VMWriter(string filename) {
		ofs.open(filename);
	}

	void WritePush(Segment segment, int index) {
		ofs << "push " << Segtostr(segment) << " " << index << endl;
	}

	void WritePop(Segment segment, int index) {
		ofs << "pop " << Segtostr(segment) << " " << index << endl;
	}

	void WriteArithmetic(Command command) {
		string com = [&]() {
			if (command == Command::ADD) return "add";
			if (command == Command::SUB) return "sub";
			if (command == Command::NEG) return "neg";
			if (command == Command::EQ) return "eq";
			if (command == Command::GT) return "gt";
			if (command == Command::LT) return "lt";
			if (command == Command::AND) return "and";
			if (command == Command::OR) return "or";
			return "not";
		}();
		ofs << com << endl;
	}

	void WriteLabel(string label) {
		ofs << "label " << label << endl;
	}

	void WriteGoto(string label) {
		ofs << "goto " << label << endl;
	}

	void WriteIf(string label) {
		ofs << "if-goto " << label << endl;
	}

	void WriteCall(string name, int nargs) {
		ofs << "call " << name << " " << nargs << endl;
	}

	void WriteFunction(string name, int nlocals) {
		ofs << "function " << name << " " << nlocals << endl;
	}

	void WriteReturn() {
		ofs << "return" << endl;
	}

	void Close() { ofs.close(); }
};

class CompilationEngine {
private:
	ofstream ofs;
	SymbolTable st;
	VMWriter vmw;
	JackTokenizer jt;
	int labelnum = 0;
	string indent;
	string nowclassname;

	void AddIndent() { indent += "  "; }

	void DeleteIndent() {
		indent.pop_back();
		indent.pop_back();
	}

	string Keytostr(Keyword keyword) {
		if (keyword == Keyword::CLASS) return "class";
		if (keyword == Keyword::CONSTRUCTOR) return "constructor";
		if (keyword == Keyword::FUNCTION) return "function";
		if (keyword == Keyword::METHOD) return "method";
		if (keyword == Keyword::FIELD) return "field";
		if (keyword == Keyword::STATIC) return "static";
		if (keyword == Keyword::VAR) return "var";
		if (keyword == Keyword::INT) return "int";
		if (keyword == Keyword::CHAR) return "char";
		if (keyword == Keyword::BOOLEAN) return "boolean";
		if (keyword == Keyword::VOID) return "void";
		if (keyword == Keyword::TRUE) return "true";
		if (keyword == Keyword::FALSE) return "false";
		if (keyword == Keyword::NULL_) return "null";
		if (keyword == Keyword::THIS) return "this";
		if (keyword == Keyword::LET) return "let";
		if (keyword == Keyword::DO) return "do";
		if (keyword == Keyword::IF) return "if";
		if (keyword == Keyword::ELSE) return "else";
		if (keyword == Keyword::WHILE) return "while";
		if (keyword == Keyword::RETURN) return "return";
	}

	void Write(string word) { ofs << indent << word << endl; }

	Keyword WriteKeyword() {
		Keyword keyword = jt.KeyWord();
		Write("<keyword> " + Keytostr(keyword) + " </keyword>");
		jt.Advance();
		return keyword;
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

	string WriteIdentifier() {
		string identifier = jt.Identifier();
		Write("<identifier> " + identifier + " </identifier>");
		jt.Advance();
		return identifier;
	}

	string WriteType() {
		if (jt.TokenType() == Token::KEYWORD) return Keytostr(WriteKeyword());
		else return WriteIdentifier();
	}

	void CallSubroutine(string identifier = "") {
		string tmp = (identifier.empty() ? WriteIdentifier() : identifier);
		if (jt.Symbol() == ".") {
			WriteSymbol();
			string funcname = WriteIdentifier();

			if (st.KindOf(tmp) == Kind::NONE) {
				string classname = tmp;

				WriteSymbol();
				int nargs = CompileExpressionList();
				WriteSymbol();

				vmw.WriteCall(classname + "." + funcname, nargs);
			} else {
				string varname = tmp;
				Segment segment = [](Kind ki) {
					if (ki == Kind::STATIC) return Segment::STATIC;
					if (ki == Kind::FIELD) return Segment::THIS;
					if (ki == Kind::ARG) return Segment::ARG;
					return Segment::LOCAL;
				}(st.KindOf(varname));

				vmw.WritePush(segment, st.IndexOf(varname));

				WriteSymbol();
				int nargs = CompileExpressionList();
				WriteSymbol();

				vmw.WriteCall(st.TypeOf(varname) + "." + funcname, nargs + 1);
			}
		} else {
			string funcname = tmp;

			vmw.WritePush(Segment::POINTER, 0);
			WriteSymbol();
			int nargs = CompileExpressionList();
			WriteSymbol();

			vmw.WriteCall(nowclassname + "." + funcname, nargs + 1);
		}
	}

	string GetLabel() {
		++labelnum;
		return "LABEL_" + to_string(labelnum);
	}

public:
	CompilationEngine(string ifilename, string ofilename) {
		jt = JackTokenizer(ifilename);
		ofs.open(ofilename + ".xml");
		vmw = VMWriter(ofilename + ".vm");

		CompileClass();

		vmw.Close();
		ofs.close();
	}

	void CompileClass() {
		Write("<class>");
		AddIndent();

		WriteKeyword();
		nowclassname = WriteIdentifier();
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

		Keyword keyword = WriteKeyword();
		Kind kind = (keyword == Keyword::STATIC ? Kind::STATIC : Kind::FIELD);
		string type = WriteType();
		string name = WriteIdentifier();
		st.Define(name, type, kind);
		
		while (jt.Symbol() != ";") {
			WriteSymbol();
			name = WriteIdentifier();
			st.Define(name, type, kind);
		}
		WriteSymbol();

		DeleteIndent();
		Write("</classVarDec>");
	}

	void CompileSubroutine() {
		Write("<subroutineDec>");
		AddIndent();

		st.StartSubroutine();

		Keyword keyword = WriteKeyword();
		if (jt.TokenType() == Token::KEYWORD && jt.KeyWord() == Keyword::VOID)
			WriteKeyword();
		else
			WriteType();
		string subroutinename = WriteIdentifier();
		
		if (keyword == Keyword::METHOD)
			st.Define("this", nowclassname, Kind::ARG);

		WriteSymbol();
		CompileParameterList();
		WriteSymbol();

		Write("<subroutineBody>");
		AddIndent();

		WriteSymbol();

		int localcnt = 0;
		while (jt.KeyWord() == Keyword::VAR)
			localcnt += CompileVarDec();
		vmw.WriteFunction(nowclassname + "." + subroutinename, localcnt);

		switch (keyword) 	{
		case Keyword::METHOD:
			vmw.WritePush(Segment::ARG, 0);
			vmw.WritePop(Segment::POINTER, 0);
			break;
		case Keyword::CONSTRUCTOR:
			vmw.WritePush(Segment::CONST, st.VarCount(Kind::FIELD));
			vmw.WriteCall("Memory.alloc", 1);
			vmw.WritePop(Segment::POINTER, 0);
			break;
		}

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
			string type = WriteType();
			string name = WriteIdentifier();
			st.Define(name, type, Kind::ARG);
			while (jt.Symbol() == ",") {
				WriteSymbol();
				type = WriteType();
				name = WriteIdentifier();
				st.Define(name, type, Kind::ARG);
			}
		}

		DeleteIndent();
		Write("</parameterList>");
	}

	int CompileVarDec() {
		Write("<varDec>");
		AddIndent();

		int cnt = 1;
		WriteKeyword();
		string type = WriteType();
		string name = WriteIdentifier();
		st.Define(name, type, Kind::VAR);
		while (jt.Symbol() == ",") {
			++cnt;
			WriteSymbol();
			name = WriteIdentifier();
			st.Define(name, type, Kind::VAR);
		}
		WriteSymbol();

		DeleteIndent();
		Write("</varDec>");

		return cnt;
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
		CallSubroutine();
		WriteSymbol();

		vmw.WritePop(Segment::TEMP, 0); // return value isn't used

		DeleteIndent();
		Write("</doStatement>");
	}

	void CompileLet() {
		Write("<letStatement>");
		AddIndent();

		WriteKeyword();
		string name = WriteIdentifier();
		Segment segment = [](Kind ki) {
			if (ki == Kind::STATIC) return Segment::STATIC;
			if (ki == Kind::FIELD) return Segment::THIS;
			if (ki == Kind::ARG) return Segment::ARG;
			return Segment::LOCAL;
		}(st.KindOf(name));

		if (jt.Symbol() == "[") {
			WriteSymbol();
			CompileExpression();
			WriteSymbol();
		
			vmw.WritePush(segment, st.IndexOf(name));
			vmw.WriteArithmetic(Command::ADD);

			WriteSymbol();
			CompileExpression();
			WriteSymbol();

			vmw.WritePop(Segment::TEMP, 0);
			vmw.WritePop(Segment::POINTER, 1);
			vmw.WritePush(Segment::TEMP, 0);
			vmw.WritePop(Segment::THAT, 0);
		} else {
			WriteSymbol();
			CompileExpression();
			WriteSymbol();

			vmw.WritePop(segment, st.IndexOf(name));
		}


		DeleteIndent();
		Write("</letStatement>");
	}

	void CompileWhile() {
		Write("<whileStatement>");
		AddIndent();

		string whilelabel = GetLabel();
		string falselabel = GetLabel();

		WriteKeyword();
		WriteSymbol();

		vmw.WriteLabel(whilelabel);
		CompileExpression();
		vmw.WriteArithmetic(Command::NOT);
		WriteSymbol();
		WriteSymbol();

		vmw.WriteIf(falselabel);
		CompileStatements();
		WriteSymbol();

		vmw.WriteGoto(whilelabel);
		vmw.WriteLabel(falselabel);

		DeleteIndent();
		Write("</whileStatement>");
	}

	void CompileReturn() {
		Write("<returnStatement>");
		AddIndent();

		WriteKeyword();
		if (jt.TokenType() != Token::SYMBOL || jt.Symbol() != ";")
			CompileExpression();
		else
			vmw.WritePush(Segment::CONST, 0);
		WriteSymbol();

		vmw.WriteReturn();

		DeleteIndent();
		Write("</returnStatement>");
	}

	void CompileIf() {
		Write("<ifStatement>");
		AddIndent();

		string elselabel = GetLabel();
		string endlabel = GetLabel();

		WriteKeyword();
		WriteSymbol();
		CompileExpression();
		vmw.WriteArithmetic(Command::NOT);

		WriteSymbol();
		WriteSymbol();
		
		vmw.WriteIf(elselabel);
		CompileStatements();
		WriteSymbol();

		vmw.WriteGoto(endlabel);
		vmw.WriteLabel(elselabel);

		if (jt.TokenType() == Token::KEYWORD && jt.KeyWord() == Keyword::ELSE) {
			WriteKeyword();
			WriteSymbol();
			CompileStatements();
			WriteSymbol();
		}

		vmw.WriteLabel(endlabel);

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
			string op = jt.Symbol();

			WriteSymbol();
			CompileTerm();

			if (op == "+") vmw.WriteArithmetic(Command::ADD);
			else if (op == "-") vmw.WriteArithmetic(Command::SUB);
			else if (op == "*") vmw.WriteCall("Math.multiply", 2);
			else if (op == "/") vmw.WriteCall("Math.divide", 2);
			else if (op == "&amp;") vmw.WriteArithmetic(Command::AND);
			else if (op == "|") vmw.WriteArithmetic(Command::OR);
			else if (op == "&lt;") vmw.WriteArithmetic(Command::LT);
			else if (op == "&gt;") vmw.WriteArithmetic(Command::GT);
			else if (op == "=") vmw.WriteArithmetic(Command::EQ);
		}

		DeleteIndent();
		Write("</expression>");
	}

	void CompileTerm() {
		Write("<term>");
		AddIndent();

		if (jt.TokenType() == Token::INT_CONST) {
			vmw.WritePush(Segment::CONST, jt.IntVal());
			WriteIntegerConstant();
		} else if (jt.TokenType() == Token::STRING_CONST) {
			string cs = jt.StringVal();
			vmw.WritePush(Segment::CONST, cs.size());
			vmw.WriteCall("String.new", 1);
			for (char& c : cs) {
				vmw.WritePush(Segment::CONST, c);
				vmw.WriteCall("String.appendChar", 2);
			}
			WriteStringConstant();
		} else if (jt.TokenType() == Token::KEYWORD) {
			switch (jt.KeyWord()) {
			case Keyword::TRUE:
				vmw.WritePush(Segment::CONST, 0);
				vmw.WriteArithmetic(Command::NOT);
				break;
			case Keyword::FALSE:
			case Keyword::NULL_:
				vmw.WritePush(Segment::CONST, 0);
				break;
			case Keyword::THIS:
				vmw.WritePush(Segment::POINTER, 0);
				break;
			}

			WriteKeyword();
		} else if (jt.TokenType() == Token::IDENTIFIER) {
			string name = WriteIdentifier();
			Segment segment = [](Kind ki) {
				if (ki == Kind::STATIC) return Segment::STATIC;
				if (ki == Kind::FIELD) return Segment::THIS;
				if (ki == Kind::ARG) return Segment::ARG;
				return Segment::LOCAL;
			}(st.KindOf(name));

			if (jt.TokenType() == Token::SYMBOL) {
				if (jt.Symbol() == "[") {
					WriteSymbol();
					CompileExpression();
					WriteSymbol();

					vmw.WritePush(segment, st.IndexOf(name));
					vmw.WriteArithmetic(Command::ADD);
					vmw.WritePop(Segment::POINTER, 1);
					vmw.WritePush(Segment::THAT, 0);
				} else if (jt.Symbol() == "(" || jt.Symbol() == ".") {
					CallSubroutine(name);
				} else {
					vmw.WritePush(segment, st.IndexOf(name));
				}
			}
		} else {
			if (jt.Symbol() == "(") {
				WriteSymbol();
				CompileExpression();
				WriteSymbol();
			} else {
				string symbol = jt.Symbol();
				WriteSymbol();
				CompileTerm();
				if (symbol == "~") vmw.WriteArithmetic(Command::NOT);
				else if (symbol == "-") vmw.WriteArithmetic(Command::NEG);
			}
		}

		DeleteIndent();
		Write("</term>");
	}

	int CompileExpressionList() {
		Write("<expressionList>");
		AddIndent();

		int nargs = 0;
		if (jt.TokenType() != Token::SYMBOL || jt.Symbol() != ")") {
			++nargs;
			CompileExpression();
			while (jt.TokenType() == Token::SYMBOL && jt.Symbol() == ",") {
				++nargs;
				WriteSymbol();
				CompileExpression();
			}
		}

		DeleteIndent();
		Write("</expressionList>");

		return nargs;
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
			CompilationEngine(file, file.substr(0, file.size() - 5));
		}
	}
};

int main(int argc, char** argv) {
	namespace fs = filesystem;
	fs::path source = fs::absolute(argv[1]).remove_filename();
	fs::path exe = fs::absolute(argv[0]).parent_path();
	fs::path os = exe / "OS";
	for (auto& p : fs::directory_iterator(os)) {
		auto to = source / p.path().filename();
		if (fs::exists(to)) continue;
		fs::copy(p, to);
	}

	JackAnalyzer ja(argv[1]);

	return 0;
}