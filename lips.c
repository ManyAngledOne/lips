#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#ifdef _WIN32
const int BUF_SIZE = 2048;

static char buffer[BUF_SIZE];

char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, BUF_SIZE, stdin);
  size_t buflen = strlen(buffer);
  char* cpy = malloc(buflen +1);
  memcpy(cpy, buffer, buflen);
  cpy[buflen] = '\0';
  return cpy;
}

void add_history(char* unused) {}

#else
#include <readline/readline.h>
#include <readline/history.h>
#endif

enum { SEXPR, SYMBOL, STRING, INT, FLOAT };

typedef struct sexpr {
  int type;
  struct sexpr* child;
  void *self;
} sexpr;

void syntax_err(const char *s) {
  fprintf(stderr, "syntax error: %.10s\n", s);
}

sexpr* parse_string(char** sptr) {
  // just think s = *sptr
  char* string = *sptr;
  while (**sptr) {
    switch(**sptr) {
      case '\\':
        switch(*++(*sptr)){
          //TODO: handle more escaped characters
          //or treat them differently...
          case '\\': case '"': case 'n': case 'r': case 't':
            continue;
          default:
            goto fail;
        }
      case '"':
        //success
        **sptr = '\0'; //terminate string
        (*sptr)++;
        sexpr* ex = malloc(sizeof(sexpr));
        ex->type = STRING;
        ex->self = string;
        return ex;
      default:
        (*sptr)++;
        continue;
    }
  }
  goto fail;

fail:
  syntax_err(*sptr);
  return 0;
}

sexpr* parse_num(char** sptr) {
  char* endptr;
  long integer = strtol(*sptr, &endptr, 10);

  if (*endptr == '.') {
    double* real = malloc(sizeof(double));
    *real = strtod(*sptr, &endptr);
    if (isspace(*endptr) || *endptr == ')') {
      sexpr* ex = malloc(sizeof(sexpr));
      ex->type = FLOAT;
      ex->self = real;
      *sptr = endptr;
      return ex;
    } else {
      free(real);
      goto fail;
    }
  } else if (isspace(*endptr) || *endptr == ')') {
    long* sto_int = malloc(sizeof(long));
    memcpy(sto_int, &integer, sizeof(long));

    sexpr* ex = malloc(sizeof(sexpr));
    ex->type = INT;
    ex->self = sto_int;
    *sptr = endptr;
    return ex;
  }
  goto fail;

fail:
  syntax_err(endptr);
  return 0;
}

sexpr* parse_symbol(char** sptr) {
  sexpr* ex;
  char* symbol = *sptr;
  while (**sptr) {
    switch(**sptr) {
      case '(':
        goto success;
      case ')':
        goto success;
      case '\\':
        switch (*++(*sptr)) { //TODO: add more esc chars
          case '\\': case '"': case '(': case ')':
            (*sptr)++;
            break;
          default:
            goto fail;
        }
      case '"':
        goto fail;
      default:
        (*sptr)++;
    }
    if (isspace(**sptr)) {
      goto success;
    }
  }

success:
  **sptr = '\0';
  (*sptr)++;
  ex = malloc(sizeof(sexpr));
  ex->type = SYMBOL;
  ex->self = symbol;
  return ex;

fail:
  syntax_err(*sptr);
  return 0;
}

sexpr* parse_sexpr(char** sptr) {
  sexpr* list = malloc(sizeof(sexpr));
  list->type = SEXPR;

  sexpr* chi;

  switch (**sptr) {
    case '('://begin list
      (*sptr)++;
      chi = parse_sexpr(sptr);
      if (!chi) goto fail;
      break;

    case ')': //end list, success
      (*sptr)++;
      list->self = NULL;
      return list;

    case '"': //begin string
      (*sptr)++;
      chi = parse_string(sptr);
      if (!chi) goto fail;
      break;

    case '.': case '+': case '-':
      if (isdigit(*((*sptr)+1))) {
        chi = parse_num(sptr);
      } else {
        chi = parse_symbol(sptr);
      }
      if (!chi) goto fail;
      break;

    default:
      if (isdigit(**sptr)){ //begin num
        chi = parse_num(sptr);
      } else { // begin symbol
        chi = parse_symbol(sptr);
      }
      if (!chi) goto fail;
  }

  list->self = chi;
  list->child = NULL;
  sexpr* parent;

  while (**sptr) {
    if (isspace(**sptr)) {
      (*sptr)++;
      continue;
    }
    parent = chi;
    switch (**sptr) {
      case '('://begin list
        (*sptr)++;
        chi = parse_sexpr(sptr);
        if (!chi) goto fail;
        break;

      case ')': //end list, success
        (*sptr)++;
        parent->child = NULL;
        return list;

      case '"': //begin string
        (*sptr)++;
        chi = parse_string(sptr);
        if (!chi) goto fail;
        break;

      case '.': case '+': case '-':
        if (isdigit(*((*sptr)+1))) {
          chi = parse_num(sptr);
        } else {
          chi = parse_symbol(sptr);
        }
        if (!chi) goto fail;
        break;

      default: //begin symbol
        if (isdigit(**sptr)) {
          chi = parse_num(sptr);
        } else {
          chi = parse_symbol(sptr);
        }
        if (!chi) goto fail;
    }
    parent->child = chi;
  }
  return list;

fail:
  syntax_err(*sptr);
  free(list); // TODO: free whole list
  return 0;
}

sexpr* parse_term(char** sptr) {
  while (**sptr) {
    if (isspace(**sptr)) {
      (*sptr)++;
      continue;
    }
    switch(**sptr) {
      case '(':
        (*sptr)++;
        return parse_sexpr(sptr);
      case '"':
        (*sptr)++;
        return parse_string(sptr);
      case '.': case '+': case '-':
        if (isdigit(*((*sptr)+1))) {
          return parse_num(sptr);
        } else {
          return parse_symbol(sptr);
        }
      default:
        if (isdigit(**sptr)) {
          return parse_num(sptr);
        } else {
          return parse_symbol(sptr);
        }
    }
  }
  return 0;
}

void print_sexpr(sexpr* ex, int depth) {
  char* pad;
  if (depth) {
    pad = malloc(sizeof(char) * depth);
    memset(pad, '\t', sizeof(char) * (depth+1));
    pad[depth] = '\0';
  } else {
    pad = "";
  }
  while (1) {
    printf("%s", pad);
    switch(ex->type) {
      case (SEXPR):
        printf("(\n");
        print_sexpr(ex->self, depth+1);
        printf(")\n");
        break;
      case (SYMBOL):
        printf("%s\n", (char*) ex->self);
        break;
      case (STRING):
        printf("\"");
        printf("%s", (char*) ex->self);
        printf("\"\n");
        break;
      case (INT):
        printf("%d\n", *((int*) ex->self));
        break;
      case (FLOAT):
        printf("%f\n", *((float*) ex->self));
        break;
      default:
        printf("Unrecognized type");
    }
    if (!ex->child) {
      return;
    }
    ex = ex->child;
  }
}

int main() {
  while(1) {
    char* in = readline("lips> ");
    add_history(in);

    while(*in){
      sexpr* x = parse_term(&in);
      print_sexpr(x, 0);
    }
  }
  return 0;
}
