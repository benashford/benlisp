#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>

#include "mpc.h"

typedef struct lval {
  int type;
  union {
	char* err;
	char* sym;
	long num;
	double floatnum;
  } val;
  int count;
  struct lval** cell;
} lval;

enum { LVAL_NUM, LVAL_FLOAT, LVAL_ERR, LVAL_SYM, LVAL_SEXPR };

enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

lval* lval_num(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->val.num = x;
  return v;
}

lval* lval_float(double x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FLOAT;
  v->val.floatnum = x;
  return v;
}

lval* lval_err(char* m) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->val.err = malloc(strlen(m) + 1);
  strcpy(v->val.err, m);
  return v;
}

lval* lval_sym(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->val.sym = malloc(strlen(s) + 1);
  strcpy(v->val.sym, s);
  return v;
}

lval* lval_sexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

void lval_del(lval* v) {
  switch (v->type) {
  case LVAL_NUM: break;
  case LVAL_FLOAT: break;
  case LVAL_ERR: free(v->val.err); break;
  case LVAL_SYM: free(v->val.sym); break;
  case LVAL_SEXPR:
	for (int i = 0; i < v->count; i++) {
	  lval_del(v->cell[i]);
	}
	free(v->cell);
	break;
  }
  free(v);
}

lval* lval_add(lval* v, lval* x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count - 1] = x;
  return v;
}

lval* lval_read_num(mpc_ast_t* t) {
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval* lval_read_float(mpc_ast_t* t) {
  double x = strtod(t->contents, NULL);
  return errno != ERANGE ? lval_float(x) : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t) {
  if (strstr(t->tag, "number")) { return lval_read_num(t); }
  if (strstr(t->tag, "floatnum")) { return lval_read_float(t); }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

  lval* x = NULL;
  if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
  if (strstr(t->tag, "sexpr")) { x = lval_sexpr(); }

  for (int i = 0; i < t->children_num; i++) {
	if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
	if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
	if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
	if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
	if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
	x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
}

void lval_print(lval* v);

void lval_expr_print(lval* v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {
	lval_print(v->cell[i]);
	if (i != (v->count - 1)) {
	  putchar(' ');
	}
  }
  putchar(close);
}

void lval_print(lval* v) {
  switch (v->type) {
  case LVAL_NUM: printf("%li", v->val.num); break;
  case LVAL_FLOAT: printf("%f", v->val.floatnum); break;
  case LVAL_ERR: printf("Error: %s", v->val.err); break;
  case LVAL_SYM: printf("%s", v->val.sym); break;
  case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
  }
}

void lval_println(lval* v) {
  lval_print(v);
  putchar('\n');
}

lval* lval_pop(lval* v, int i) {
  lval* x = v->cell[i];
  memmove(&v->cell[i], &v->cell[i + 1], sizeof(lval*) * (v->count - i - 1));
  v->count--;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  return x;
}

lval* lval_take(lval* v, int i) {
  lval* x = lval_pop(v, i);
  lval_del(v);
  return x;
}

lval* builtin_op(lval* v, char* op);

lval* lval_eval(lval* v);

lval* lval_eval_sexpr(lval* v) {
  for (int i = 0; i < v->count; i++ ) {
	v->cell[i] = lval_eval(v->cell[i]);
  }

  for (int i = 0; i < v->count; i++) {
	if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
  }

  if (v->count == 0) { return v; }
  if (v->count == 1) { return lval_take(v, 0); }

  lval* f = lval_pop(v, 0);
  if (f->type != LVAL_SYM) {
	lval_del(f);
	lval_del(v);
	return lval_err("S-expression does not begin with a symbol");
  }

  lval* result = builtin_op(v, f->val.sym);
  lval_del(f);
  return result;
}

lval* lval_eval(lval* v) {
  if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
  return v;
}

lval* builtin_add(lval* x, lval* y) {
  if (x->type == LVAL_NUM && y->type == LVAL_NUM) {
	x->val.num += y->val.num;
	return x;
  } else {
	double xd;
	if (x->type == LVAL_NUM) {
	  xd = (double)x->val.num;
	} else if (x->type == LVAL_FLOAT) {
	  xd = x->val.floatnum;
	} else {
	  lval_del(x);
	  return lval_err("TYPE MISMATCH");
	}

	double yd;
	if (y->type == LVAL_NUM) {
	  yd = (double)y->val.num;
	} else if (y->type == LVAL_FLOAT) {
	  yd = y->val.floatnum;
	} else {
	  lval_del(x);
	  return lval_err("TYPE MISMATCH");
	}
	lval_del(x);
	return lval_float(xd + yd);
  }
}

lval* builtin_mult(lval* x, lval* y) {
  if (x->type == LVAL_NUM && y->type == LVAL_NUM) {
	x->val.num *= y->val.num;
	return x;
  } else {
	double xd;
	if (x->type == LVAL_NUM) {
	  xd = (double)x->val.num;
	} else if (x->type == LVAL_FLOAT) {
	  xd = x->val.floatnum;
	} else {
	  lval_del(x);
	  return lval_err("TYPE MISMATCH");
	}

	double yd;
	if (y->type == LVAL_NUM) {
	  yd = (double)y->val.num;
	} else if (y->type == LVAL_FLOAT) {
	  yd = y->val.floatnum;
	} else {
	  lval_del(x);
	  return lval_err("TYPE MISMATCH");
	}
	lval_del(x);
	return lval_float(xd * yd);
  }
}

lval* builtin_minus(lval* x, lval* y) {
  if (x->type == LVAL_NUM && y->type == LVAL_NUM) {
	x->val.num -= y->val.num;
	return x;
  } else {
	double xd;
	if (x->type == LVAL_NUM) {
	  xd = (double)x->val.num;
	} else if (x->type == LVAL_FLOAT) {
	  xd = x->val.floatnum;
	} else {
	  lval_del(x);
	  return lval_err("TYPE MISMATCH");
	}

	double yd;
	if (y->type == LVAL_NUM) {
	  yd = (double)y->val.num;
	} else if (y->type == LVAL_FLOAT) {
	  yd = y->val.floatnum;
	} else {
	  lval_del(x);
	  return lval_err("TYPE MISMATCH");
	}
	lval_del(x);
	return lval_float(xd - yd);
  }
}

lval* builtin_div(lval* x, lval* y) {
  if (x->type == LVAL_NUM && y->type == LVAL_NUM) {
	if (y->val.num == 0) {
	  lval_del(x);
	  return lval_err("Division By Zero!");
	} else {
	  x->val.num /= y->val.num;
	  return x;
	}
  } else {
	double xd;
	if (x->type == LVAL_NUM) {
	  xd = (double)x->val.num;
	} else if (x->type == LVAL_FLOAT) {
	  xd = x->val.floatnum;
	} else {
	  lval_del(x);
	  return lval_err("TYPE MISMATCH");
	}

	double yd;
	if (y->type == LVAL_NUM) {
	  yd = (double)y->val.num;
	} else if (y->type == LVAL_FLOAT) {
	  yd = y->val.floatnum;
	} else {
	  lval_del(x);
	  return lval_err("TYPE MISMATCH");
	}
	lval_del(x);
	return lval_float(xd / yd);
  }
}

lval* builtin_mod(lval* x, lval* y) {
  if (x->type == LVAL_NUM && y->type == LVAL_NUM) {
	x->val.num = x->val.num % y->val.num;
	return x;
  } else {
	lval_del(x);
	return lval_err("TYPE MISMATCH");
  }
}

lval* builtin_pow(lval* x, lval* y) {
  if (x->type == LVAL_NUM && y->type == LVAL_NUM) {
	x->val.num = pow(x->val.num, y->val.num);
	return x;
  } else {
	double xd;
	if (x->type == LVAL_NUM) {
	  xd = (double)x->val.num;
	} else if (x->type == LVAL_FLOAT) {
	  xd = x->val.floatnum;
	} else {
	  lval_del(x);
	  return lval_err("TYPE MISMATCH");
	}

	double yd;
	if (y->type == LVAL_NUM) {
	  yd = (double)y->val.num;
	} else if (y->type == LVAL_FLOAT) {
	  yd = y->val.floatnum;
	} else {
	  lval_del(x);
	  return lval_err("TYPE MISMATCH");
	}
	lval_del(x);
	return lval_float(pow(xd, yd));
  }
}

lval* builtin_op(lval* a, char* op) {
  lval* x = lval_pop(a, 0);

  if ((strcmp(op, "-") == 0) && a->count == 0) { x->val.num = -x->val.num; }

  while (a->count > 0) {
	lval* y = lval_pop(a, 0);
	if (strcmp(op, "+") == 0) { x = builtin_add(x, y); }
	if (strcmp(op, "-") == 0) { x = builtin_minus(x, y); }
	if (strcmp(op, "*") == 0) { x = builtin_mult(x, y); }
	if (strcmp(op, "/") == 0) { x = builtin_div(x, y); }
	if (strcmp(op, "%") == 0) { x = builtin_mod(x, y); }
	if (strcmp(op, "^") == 0) { x = builtin_pow(x, y); }

	lval_del(y);

	if (x->type == LVAL_ERR) {
	  break;
	}
  }

  lval_del(a);
  return x;
}

int main(int argc, char** argv) {
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Floatnum = mpc_new("floatnum");
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* Sexpr = mpc_new("sexpr");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Lispy = mpc_new("lispy");

  mpca_lang(MPC_LANG_DEFAULT,
			"floatnum : /-?[0-9]+\\.[0-9]+/ ;                                \
			 number   : /-?[0-9]+/ ;                                         \
			 symbol   : '+' | '-' | '*' | '/' | '%' | '^' ;                  \
			 sexpr    : '(' <expr>* ')' ;                                    \
			 expr     : <floatnum> | <number> | <symbol> | <sexpr> ;         \
			 lispy    : /^/ <expr>* /$/ ;                                    ",
			Floatnum,
			Number,
			Symbol,
			Sexpr,
			Expr,
			Lispy);

  puts("benlisp Version 0.0.1");
  puts("Press Ctrl-C to exit\n");

  while(1) {
	char* input = readline("benlisp> ");
	add_history(input);

	mpc_result_t r;
	if (mpc_parse("<stdin>", input, Lispy, &r)) {
	  lval* x = lval_eval(lval_read(r.output));
	  lval_println(x);
	  lval_del(x);
	  mpc_ast_delete(r.output);
	} else {
	  mpc_err_print(r.error);
	  mpc_err_delete(r.error);
	}

	free(input);
  }

  mpc_cleanup(6, Number, Floatnum, Symbol, Sexpr, Expr, Lispy);

  return 0;
}
