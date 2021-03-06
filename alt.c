/* alt - abstract language tree // pancake<at>nopcode.org */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "alt.h"

int parse_is_operator(char ch) {
	return (ch=='+'||ch=='-'||ch=='&'||ch=='='||ch=='&'||ch=='~'||ch=='>'||
		ch=='|'||ch=='^'||ch=='/'||ch=='%'||ch=='*'||ch=='!'||ch=='<');
}

void parse_pushcomment(AltState *st, char ch) {
	st->str[st->stridx-1] = 0;
	memmove (st->str, st->str+1, strlen (st->str));
	if (st->cb_comment)
		st->cb_comment (st, ch);
	st->str[0] = 0;
	st->stridx = 0;
}

void parse_pushword(AltState *st, char ch) {
	if (st->stridx == 0 && ch != ':' && st->mode != MODE_STRING)
		return;
	st->str[st->stridx] = 0;
	st->cb_word (st, ch);
	st->str[0] = 0;
	st->stridx = 0;
}

int parse_concatchar(AltState *st, char ch) {
	st->str[st->stridx++] = ch;
	if (st->stridx>=ALT_MAX_LEVEL)
		return st->cb_error (st, "Too long string"); //, st->str);
	return 1; // return 0 makes it fail
}

int parse_char(AltState *st, char ch) {
	int ret = 1;
	if (ch == 0)
		return 0;

	if (st->skipuntil) {
		if (ch != st->skipuntil) {
			parse_concatchar (st, ch);
			return 1;
		}
		st->stridx++;
		parse_pushcomment (st, ch);
		st->skipuntil = 0;
	}
	st->curchar = ch;
	switch (st->mode) {
	case MODE_PARSE:
		switch (ch) {
		case '"':
		case '\'':
			if (st->lastchar == '\\')
				parse_concatchar (st, ch);
			else st->mode = MODE_STRING;
			st->endch = ch;
			break;
		case '#':
			st->skipuntil = '\n';
			break;
		case '[':
		case '(':
		case '{':
			parse_pushword (st, ch);
			if (st->cb_level)
				st->cb_level (st, 1, ch);
			switch(ch) {
			case '{': st->levels[st->level] = '}'; break;
			case '(': st->levels[st->level] = ')'; break;
			case '[': st->levels[st->level] = ']'; break;
			}
			if (++st->level > ALT_MAX_LEVEL)
				return st->cb_error (st, "Too much recursivity");
			break;
		case ']':
		case ')':
		case '}':
			parse_pushword (st, ch);
			st->level--;
			if (st->level<0)
				return st->cb_error (st, "Level underflow\n");
			if (st->levels[st->level] != ch)
				return st->cb_error (st,
					"Unexpected closing parentesis.\n");
			if (st->cb_level)
				st->cb_level (st, -1, ch);
			break;
		case '\n':
			st->line++;
		case ' ':
		case ',':
		case ';':
		case ':':
		//case '\'':
		case '\t':
		case '\r':
			parse_pushword (st, ch);
			break;
		default:
			if (st->lastchar == '/') {
				switch (ch) {
				case '*':
					st->mode = MODE_COMMENT;
					parse_pushword (st, ch);
					st->mode = MODE_PARSE;
					break;
				case '/':
					st->skipuntil = '\n';
					break;
				}
			} else {
				if (parse_is_operator (ch)) {
					st->mode = MODE_OPERATOR; // XXX dupped
					if (st->stridx>0)
						parse_pushword (st, ch);
					return parse_char (st, ch);
				} else parse_concatchar (st, ch);
			}
			break;
		}
		break;
	case MODE_OPERATOR:
		if (st->lastchar == '/') {
			switch (ch) {
			case '*':
				st->mode = MODE_COMMENT;
				st->stridx = 0;
				return parse_char (st, ch);
			case '/':
				st->skipuntil = '\n';
				st->mode = MODE_COMMENT;
				st->stridx = 0;
				break;
			}
		}
		if (st->mode != MODE_OPERATOR || !parse_is_operator (ch)) {
			st->mode = MODE_PARSE;
			parse_pushword (st, ch);
			// XXX: check if return here is ok
			return parse_char (st, ch);
		} else parse_concatchar (st, ch);
		break;
	case MODE_COMMENT:
		if (st->lastchar == '*' && ch == '/') {
			parse_pushcomment (st, ch);
			st->mode = MODE_PARSE;
			st->stridx = ch = 0;
			ch = 0; // workaround for default->lastchar==/
		} else parse_concatchar (st, ch);
		break;
	case MODE_STRING:
		if (st->lastchar == '\\') {
			switch (ch) {
			case 'n': ch = '\n'; break;
			case 't': ch = '\t'; break;
			case 'r': ch = '\r'; break;
			case 'e': ch = '\x1b'; break;
			case 'x': ch = 'x'; break; // TODO: fix support for binary stuff
			case '\\': ch = '\\'; break;
			case '\'': ch = '\''; break;
			case '\"': ch = '\"'; break;
			default: //return st->cb_error(st,
				fprintf (stderr, "Invalid escaped char '%c'\n", ch);
			}
			st->str[st->stridx-1] = ch;
			if (ch == '\\') ch = 0;
		} else if (ch == st->endch) {
			parse_pushword (st, ch);
			st->mode = MODE_PARSE;
		} else ret = parse_concatchar (st, ch);
		break;
	}
	st->lastchar = ch;
	return ret;
}

int parse_str(AltState *st, char *str) {
	while (parse_char (st, *str))
		str++;
	return 0;
}

int parse_fd(AltState *st, int fd) {
	int ret, i;
	char buf[1024];
	do {
		ret = read (fd, buf, sizeof (buf));
		for (i=0; i<ret; i++)
			if (!parse_char (st, buf[i]))
				return 0;
	} while (ret>0);
	return (ret==-1);
}

int parse_file(AltState *st, const char *file) {
	int fd = open (file, O_RDONLY);
	if (fd == -1)
		return 1;
	parse_fd (st, fd);
	close (fd);
	return 0;
}
