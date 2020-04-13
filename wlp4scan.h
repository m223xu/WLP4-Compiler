#include "pch.h"


#ifndef CS241_SCANNER_H
#define CS241_SCANNER_H
#include <string>
#include <vector>
#include <set>
#include <cstdint>
#include <ostream>


class Token;


std::vector<Token> scan(const std::string &input);


class Token {
public:
	enum Kind {
		KEYWORD = 0,
		NUM,
		LPAREN,
		RPAREN,
		LBRACE,
		RBRACE,
		BECOMES,
		EQ,
		NE,
		LT,
		GT,
		LE,
		GE,
		PLUS,
		MINUS,
		STAR,
		SLASH,
		PCT,
		COMMA,
		SEMI,
		LBRACK,
		RBRACK,
		AMP,
		WHITESPACE,
		COMMENT
	};

private:
	Kind kind;
	std::string lexeme;

public:
	Token(Kind kind, std::string lexeme);

	Kind getKind() const;
	const std::string &getLexeme() const;

};

std::ostream &operator<<(std::ostream &out, const Token &tok);

/* An exception class thrown when an error is encountered while scanning.
 */
class ScanningFailure {
	std::string message;

public:
	ScanningFailure(std::string message);

	// Returns the message associated with the exception.
	const std::string &what() const;
};

#endif
