#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ------------------ basic types ------------------------
typedef enum {
  PVAL_NUMBER,
  PVAL_BOOL,
  PVAL_SYMBOL,
  PVAL_LIST,
  PVAL_FUNCTION,
  PVAL_ERROR
} pval_type;

typedef struct pval {
  pval_type type;
  void* data;
} pval;

typedef struct {
  pval** items;
  int size;
  int capacity;
} res_array;

typedef struct {
  char* name;
  pval* (*func)(pval*);
} fpval;

typedef struct env {
  pval** keys;
  pval** values;
  int size;
  int capacity;
  struct env* parent;
} env;

// --------------- constructors ---------------------
pval* pval_number(int64_t n) {
  int64_t* d = malloc(sizeof(int64_t));
  assert(d);
  *d = n;
  pval* pv = malloc(sizeof(pval));
  assert(pv);
  pv->type = PVAL_NUMBER;
  pv->data = d;
  return pv;
}

pval* pval_bool(bool b) {
  bool* d = malloc(sizeof(bool));
  assert(d);
  *d = b;
  pval* pv = malloc(sizeof(pval));
  assert(pv);
  pv->type = PVAL_BOOL;
  pv->data = d;
  return pv;
}

pval* pval_symbol(char* s) {
  char* d = malloc(strlen(s) + 1);
  assert(d);
  strcpy(d, s);
  pval* pv = malloc(sizeof(pval));
  assert(pv);
  pv->type = PVAL_SYMBOL;
  pv->data = d;
  return pv;
}

pval* pval_list() {
  res_array* d = malloc(sizeof(res_array));
  assert(d);
  d->size = 0;
  d->capacity = 4;
  d->items = malloc(d->capacity * sizeof(pval*));
  assert(d->items);
  pval* pv = malloc(sizeof(pval));
  assert(pv);
  pv->type = PVAL_LIST;
  pv->data = d;
  return pv;
}

pval* pval_error(pval* error) {
  pval* pv = malloc(sizeof(pval));
  assert(pv);
  pv->type = PVAL_ERROR;
  pv->data = error;
  return pv;
}

pval* pval_function(char* name, pval* (*func)(pval*)) {
  fpval* d = malloc(sizeof(fpval));
  assert(d);
  d->name = malloc(strlen(name) + 1);
  assert(d->name);
  strcpy(d->name, name);
  d->func = func;
  pval* pv = malloc(sizeof(pval));
  assert(pv);
  pv->type = PVAL_FUNCTION;
  pv->data = d;
  return pv;
}

env* env_new(env* parent) {
  env* e = malloc(sizeof(env));
  assert(e);
  e->parent = parent;
  e->size = 0;
  e->capacity = 4;
  e->keys = malloc(e->capacity * sizeof(pval*));
  e->values = malloc(e->capacity * sizeof(pval*));
  assert(e->keys);
  assert(e->values);
  return e;
}

//-------------------- pval helpers --------------------------------
void pval_delete(pval* pv) {
  if (!pv)
    return;
  switch (pv->type) {
  case PVAL_NUMBER:
    free(pv->data);
    free(pv);
    break;
  case PVAL_BOOL:
    free(pv->data);
    free(pv);
    break;
  case PVAL_SYMBOL:
    free(pv->data);
    free(pv);
    break;
  case PVAL_LIST: {
    res_array* d = (res_array*)pv->data;
    for (int i = 0; i < d->size; i++) {
      pval_delete(d->items[i]);
    }
    free(d->items);
    free(d);
    free(pv);
    break;
  }
  case PVAL_FUNCTION: {
    fpval* d = (fpval*)pv->data;
    free(d->name);
    free(d);
    free(pv);
    break;
  }
  case PVAL_ERROR: {
    pval* error = (pval*)pv->data;
    pval_delete(error);
    free(pv);
    break;
  }
  default:
    free(pv);
    break;
  }
}

void pval_print(pval* pv) {
  switch (pv->type) {
  case PVAL_NUMBER:
    printf("%lld", *((int64_t*)pv->data));
    break;
  case PVAL_BOOL:
    if (*((bool*)pv->data)) {
      printf("#t");
    } else {
      printf("#f");
    }
    break;
  case PVAL_SYMBOL:
    printf("%s", (char*)pv->data);
    break;
  case PVAL_LIST: {
    res_array* d = (res_array*)pv->data;
    printf("(");
    for (int i = 0; i < d->size; i++) {
      if (i > 0)
        printf(" ");
      pval_print(d->items[i]);
    }
    printf(")");
    break;
  }
  case PVAL_FUNCTION: {
    fpval* d = (fpval*)pv->data;
    printf("$builtin{%s}", d->name);
    break;
  }
  case PVAL_ERROR:
    printf("$error{");
    pval_print((pval*)pv->data);
    printf("}");
    break;
  default:
    break;
  }
}

void pval_list_add(pval* list, pval* elem) {
  res_array* d = (res_array*)list->data;
  if (d->size == d->capacity) {
    d->capacity *= 2;
    d->items = realloc(d->items, d->capacity * sizeof(pval*));
    assert(d->items);
  }
  d->items[d->size] = elem;
  d->size += 1;
}

pval* pval_copy(pval* pv) {
  if (!pv)
    return pval_error(pval_symbol("null-copy"));
  switch (pv->type) {
  case PVAL_NUMBER:
    return pval_number(*((int64_t*)pv->data));
  case PVAL_BOOL:
    return pval_bool(*((bool*)pv->data));
  case PVAL_SYMBOL:
    return pval_symbol((char*)pv->data);
  case PVAL_LIST: {
    res_array* d = (res_array*)pv->data;
    pval* copy = pval_list();
    for (int i = 0; i < d->size; i++) {
      pval_list_add(copy, pval_copy(d->items[i]));
    }
    return copy;
  }
  case PVAL_FUNCTION: {
    fpval* d = (fpval*)pv->data;
    return pval_function(d->name, d->func);
  }
  case PVAL_ERROR:
    return pval_error(pval_copy((pval*)pv->data));
  default:
    return pval_error(pval_symbol("unknown-type"));
  }
}

//------------------------ parser ----------------------------
// forward declaration
pval* pval_parse(char* input, int* idx);

pval* parse_number(char* input, int* idx) {
  int j = 0;
  char buf[32];
  // handle signed ints
  if (input[*idx] == '-') {
    buf[j] = input[*idx];
    j++;
    (*idx)++;
  }
  while (isdigit(input[*idx])) {
    buf[j] = input[*idx];
    j++;
    (*idx)++;
  }
  buf[j] = '\0';
  int64_t n = atoll(buf);
  return pval_number(n);
}

pval* parse_symbol(char* input, int* idx) {
  int j = 0;
  char buf[64];
  while (input[*idx] != '\0' && !isspace(input[*idx]) && input[*idx] != '(' &&
         input[*idx] != ')') {
    buf[j] = input[*idx];
    j++;
    (*idx)++;
  }
  buf[j] = '\0';
  return pval_symbol(buf);
}

pval* parse_bool(char* input, int* idx) {
  if (input[*idx + 1] == 't') {
    (*idx) += 2;
    return pval_bool(true);
  } else if (input[*idx + 1] == 'f') {
    (*idx) += 2;
    return pval_bool(false);
  } else {
    return pval_error(pval_symbol("invalid-token"));
  }
}

pval* parse_list(char* input, int* idx) {
  pval* list = pval_list();
  (*idx)++;
  while (input[*idx] != ')') {
    if (input[*idx] == '\0') {
      // avoid memory leak
      pval_delete(list);
      return pval_error(pval_symbol("incomplete-parse"));
    }
    pval* elem = pval_parse(input, idx);
    pval_list_add(list, elem);
  }
  (*idx)++;
  return list;
}

// Reads one complete token at a time, returns a PSI value
pval* pval_parse(char* input, int* idx) {
  // handle multiple whitespaces
  while (isspace(input[*idx]))
    (*idx)++;
  //
  if (isdigit(input[*idx]) || (input[*idx] == '-' && isdigit(input[*idx + 1])))
    return parse_number(input, idx);
  else if (isalpha(input[*idx]) || strchr("+-*/=<>!%:", input[*idx]))
    return parse_symbol(input, idx);
  else if (input[*idx] == '#')
    return parse_bool(input, idx);
  else if (input[*idx] == '(')
    return parse_list(input, idx);
  else if (input[*idx] == '\0')
    return NULL;
  else
    return pval_error(pval_symbol("invalid-token"));
}

//------------------------ built in functions ----------------------------
pval* builtin_add(pval* args) {
  int64_t sum = 0;
  res_array* d = (res_array*)args->data;
  for (int i = 0; i < d->size; i++) {
    if (d->items[i]->type != PVAL_NUMBER)
      return pval_error(pval_symbol("type-error"));
    else
      sum += *((int64_t*)d->items[i]->data);
  }
  return pval_number(sum);
}

pval* builtin_sub(pval* args) {
  res_array* d = (res_array*)args->data;
  if (d->size == 0)
    return pval_error(pval_symbol("arity-error"));
  if (d->items[0]->type != PVAL_NUMBER)
    return pval_error(pval_symbol("type-error"));
  int64_t diff = *((int64_t*)d->items[0]->data);
  if (d->size == 1)
    return pval_number(-diff);
  for (int i = 1; i < d->size; i++) {
    if (d->items[i]->type != PVAL_NUMBER)
      return pval_error(pval_symbol("type-error"));
    else
      diff -= *((int64_t*)d->items[i]->data);
  }
  return pval_number(diff);
}

pval* builtin_mul(pval* args) {
  int64_t prod = 1;
  res_array* d = (res_array*)args->data;
  for (int i = 0; i < d->size; i++) {
    if (d->items[i]->type != PVAL_NUMBER)
      return pval_error(pval_symbol("type-error"));
    else
      prod *= *((int64_t*)d->items[i]->data);
  }
  return pval_number(prod);
}

pval* builtin_div(pval* args) {
  res_array* d = (res_array*)args->data;
  if (d->size < 2)
    return pval_error(pval_symbol("arity-error"));
  if (d->items[0]->type != PVAL_NUMBER)
    return pval_error(pval_symbol("type-error"));
  int64_t quot = *((int64_t*)d->items[0]->data);
  for (int i = 1; i < d->size; i++) {
    if (d->items[i]->type != PVAL_NUMBER)
      return pval_error(pval_symbol("type-error"));
    if (*((int64_t*)d->items[i]->data) == 0)
      return pval_error(pval_symbol("division-by-zero"));
    quot /= *((int64_t*)d->items[i]->data);
  }
  return pval_number(quot);
}

pval* builtin_eq(pval* args) {
  res_array* d = (res_array*)args->data;
  if (d->size <= 1)
    return pval_bool(true);
  pval* ref = d->items[0];
  for (int i = 1; i < d->size; i++) {
    pval* comp = d->items[i];
    if (ref->type != comp->type)
      return pval_bool(false);
    switch (ref->type) {
    case PVAL_NUMBER:
      if (*((int64_t*)ref->data) != *((int64_t*)comp->data))
        return pval_bool(false);
      break;
    case PVAL_BOOL:
      if (*((bool*)ref->data) != *((bool*)comp->data))
        return pval_bool(false);
      break;
    case PVAL_SYMBOL:
      if (strcmp((char*)ref->data, (char*)comp->data) != 0)
        return pval_bool(false);
      break;
    default:
      return pval_bool(false);
    }
  }
  return pval_bool(true);
}

pval* builtin_quit(pval* args) { exit(0); }
//------------------------- env helpers -------------------------
void env_free(env* e) {
  if (!e)
    return;
  for (int i = 0; i < e->size; i++) {
    pval_delete(e->keys[i]);
    pval_delete(e->values[i]);
  }
  free(e->keys);
  free(e->values);
  free(e);
}

// percolates up thru parent envs until val is found or unbound
pval* env_lookup(env* e, pval* key) {
  if (!e)
    return pval_error(pval_symbol("unbound"));
  for (int i = 0; i < e->size; i++) {
    if (strcmp((char*)e->keys[i]->data, (char*)key->data) == 0) {
      return pval_copy(e->values[i]);
    }
  }
  return env_lookup(e->parent, key);
}

// updates or adds val to env
void env_bind(env* e, pval* key, pval* val) {
  for (int i = 0; i < e->size; i++) {
    if (strcmp((char*)e->keys[i]->data, (char*)key->data) == 0) {
      pval_delete(e->values[i]);
      e->values[i] = pval_copy(val);
      return;
    }
  }
  if (e->size == e->capacity) {
    e->capacity *= 2;
    e->keys = realloc(e->keys, e->capacity * sizeof(pval*));
    e->values = realloc(e->values, e->capacity * sizeof(pval*));
  }
  e->keys[e->size] = pval_copy(key);
  e->values[e->size] = pval_copy(val);
  e->size++;
}

env* env_init(fpval* builtins, int size) {
    env* e = env_new(NULL);
    for (int i = 0; i < size; i++) {
        pval* key = pval_symbol(builtins[i].name);
        pval* val = pval_function(builtins[i].name, builtins[i].func);
        env_bind(e, key, val);
        pval_delete(key);
        pval_delete(val);
    }
    return e;
}

//---------------------evaluator helpers----------------------
// forward declaration
pval* psi_eval(pval* pv, env* e);
pval* psi_eval_list(pval* list, env* e) {
  res_array* d = (res_array*)list->data;
  pval* args = pval_list();
  for (int i = 0; i < d->size; i++) {
    pval* val = psi_eval(d->items[i], e);
    if (val->type == PVAL_ERROR) {
      pval_delete(args);
      return val;
    }
    pval_list_add(args, val);
  }
  return args;
}

pval* psi_apply(pval* func, pval* args) {
  if (func->type != PVAL_FUNCTION)
    return pval_error(pval_symbol("inapplicable-head"));
  fpval* f = (fpval*)func->data;
  return f->func(args);
}

//------------------------evaluator----------------------------
pval* psi_eval(pval* pv, env* e) {
  switch (pv->type) {
  case PVAL_NUMBER:
  case PVAL_BOOL:
  case PVAL_ERROR:
  case PVAL_FUNCTION:
    return pval_copy(pv);
  case PVAL_SYMBOL:
    return env_lookup(e, pv);
  case PVAL_LIST: {
    res_array* d = (res_array*)pv->data;
    if (d->size == 0)
      return pval_copy(pv);
    pval* args = psi_eval_list(pv, e);
    if (args->type == PVAL_ERROR)
      return args;
    pval* func = pval_copy(((res_array*)args->data)->items[0]);
    pval* args1 = pval_list();
    for (int i = 1; i < ((res_array*)args->data)->size; i++) {
      pval_list_add(args1, pval_copy(((res_array*)args->data)->items[i]));
    }
    pval_delete(args);
    pval* result = psi_apply(func, args1);
    pval_delete(func);
    pval_delete(args1);
    return result;
  }
  default:
    return pval_error(pval_symbol("unknown-type"));
  }
}

//----------------------repl loop------------------------------
void psi_repl(env* e) {
  while (1) {
    char input[4096];
    printf("psi> ");
    fflush(stdout);
    fgets(input, 4096, stdin);
    int idx = 0;
    pval* pv = pval_parse(input, &idx);
    if (!pv)
      continue;
    pval* result = psi_eval(pv, e);
    pval_print(result);
    printf("\n");
    pval_delete(pv);
    pval_delete(result);
  }
}

//-------------------------- main -----------------------------
fpval global_env[] = {{"+", builtin_add}, {"-", builtin_sub},
                      {"*", builtin_mul}, {"/", builtin_div},
                      {"=", builtin_eq},  {"quit", builtin_quit}};
int global_env_size = 6;

int main() { 
    env* e = env_init(global_env, global_env_size);
    psi_repl(e);
 }