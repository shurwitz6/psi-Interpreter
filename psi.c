#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ------------------ basic types ------------------------
// PSI types
typedef enum {
  PVAL_NUMBER,
  PVAL_BOOL,
  PVAL_SYMBOL,
  PVAL_LIST,
  PVAL_FUNCTION,
  PVAL_ERROR
} pval_type;

// PSI obj
typedef struct pval {
  pval_type type;
  void* data;
} pval;

// Resizable pval* array
typedef struct {
  pval** items;
  int size;
  int capacity;
} res_array;

// Builtin functions, stores name and func pointer
typedef struct {
  char* name;
  pval* (*func)(pval*);
} fpval;

// Environment, maps key to values, includes parent env
typedef struct env {
  pval** keys;
  pval** values;
  int size;
  int capacity;
  struct env* parent;
} env;

// --------------- constructors ---------------------
// Create PSI number from 64-bit signed integer
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

// Create PSI boolean (#t or #f)
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

// Create PSI symbol value from string
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

// Create empty resizable PSI list 
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

// Create PSI error
pval* pval_error(pval* error) {
  pval* pv = malloc(sizeof(pval));
  assert(pv);
  pv->type = PVAL_ERROR;
  pv->data = error;
  return pv;
}

// Create PSI builtin function obj
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

// Create new environment
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

//------------------ forward declarations -------------------
pval* pval_parse(char* input, int* idx);
pval* psi_eval(pval* pv, env* e);
pval* eval_if(pval* list, env* e);
pval* eval_def(pval* list, env* e);
pval* eval_quote(pval* list, env* e);

//-------------------- pval helpers --------------------------------
// Free pval memory
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

// Print pval
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

// Append pval to list
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

// Deep copy pval
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
// Parse signed int from input
pval* parse_number(char* input, int* idx) {
  int j = 0;
  char buf[32];
  // Handle signed ints
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

// Parse symbol from input
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

// Parse boolean from input
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

// Parse list from input
pval* parse_list(char* input, int* idx) {
  pval* list = pval_list();
  (*idx)++;
  while (input[*idx] != ')') {
    // Unclosed list
    if (input[*idx] == '\0') {
      pval_delete(list);
      return pval_error(pval_symbol("incomplete-parse"));
    }
    pval* elem = pval_parse(input, idx);
    // Invalid token inside list
    if (!elem || elem->type == PVAL_ERROR) {
      pval_delete(list);
      return elem ? elem : pval_error(pval_symbol("incomplete-parse"));
    }
    pval_list_add(list, elem);
  }
  (*idx)++;
  return list;
}

// Handle quote ' i.e. wraps next pval in (quote <pval>)
pval* parse_quote(char* input, int* idx) {
  (*idx)++;
  pval* inner = pval_parse(input, idx);
  if (!inner)
    return pval_error(pval_symbol("incomplete-parse"));
  pval* quoted = pval_list();
  pval_list_add(quoted, pval_symbol("quote"));
  pval_list_add(quoted, inner);
  return quoted;
}

// Main parser 
pval* pval_parse(char* input, int* idx) {
  // Handle multiple whitespaces
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
  else if (input[*idx] == '\'')
    return parse_quote(input, idx);
  else
    return pval_error(pval_symbol("invalid-token"));
}

//------------------------ Built in functions ----------------------------
// Return sum 
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

// Flip sign or return difference 
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

// Return product
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

// Return quotient
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

// Return #t if all arguments are equal
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

// Return everything except first element
pval* builtin_tail(pval* args) {
  res_array* d = (res_array*)args->data;
  if (d->size != 1)
    return pval_error(pval_symbol("arity-error"));
  if (d->items[0]->type != PVAL_LIST)
    return pval_error(pval_symbol("type-error"));
  res_array* inner = (res_array*)d->items[0]->data;
  if (inner->size == 0)
    return pval_error(pval_symbol("value-error"));
  pval* result = pval_list();
  for (int i = 1; i < inner->size; i++) {
    pval_list_add(result, pval_copy(inner->items[i]));
  }
  return result;
}

// Prepend value to a list
pval* builtin_cons(pval* args) {
  res_array* d = (res_array*)args->data;
  if (d->size != 2)
    return pval_error(pval_symbol("arity-error"));
  if (d->items[1]->type != PVAL_LIST)
    return pval_error(pval_symbol("type-error"));
  pval* result = pval_list();
  pval_list_add(result, pval_copy(d->items[0]));
  res_array* tail = (res_array*)d->items[1]->data;
  for (int i = 0; i < tail->size; i++) {
    pval_list_add(result, pval_copy(tail->items[i]));
  }
  return result;
}

// Return first element of a list
pval* builtin_head(pval* args) {
  res_array* d = (res_array*)args->data;
  if (d->size != 1)
    return pval_error(pval_symbol("arity-error"));
  if (d->items[0]->type != PVAL_LIST)
    return pval_error(pval_symbol("type-error"));
  res_array* inner = (res_array*)d->items[0]->data;
  if (inner->size == 0)
    return pval_error(pval_symbol("value-error"));
  return pval_copy(inner->items[0]);
}

// Return type 
pval* builtin_type(pval* args) {
  res_array* d = (res_array*)args->data;
  if (d->size != 1)
    return pval_error(pval_symbol("arity-error"));
  switch (d->items[0]->type) {
  case PVAL_NUMBER:
    return pval_symbol("number");
  case PVAL_BOOL:
    return pval_symbol("bool");
  case PVAL_SYMBOL:
    return pval_symbol("symbol");
  case PVAL_LIST:
    return pval_symbol("list");
  case PVAL_FUNCTION:
    return pval_symbol("function");
  case PVAL_ERROR:
    return pval_symbol("error");
  default:
    return pval_error(pval_symbol("unknown-type"));
  }
}

// Exit w/ return code 0
pval* builtin_quit(pval* args) { exit(0); }

//------------------------- env helpers -------------------------
// Free memory owned by environment
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

// Lookup symbol in the environment
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

// Add/update key-value to environment
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

// Create glob env from builtin functions
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
// Evaluate each element in list
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

// Apply function pval to list
pval* psi_apply(pval* func, pval* args) {
  if (func->type != PVAL_FUNCTION)
    return pval_error(pval_symbol("inapplicable-head"));
  fpval* f = (fpval*)func->data;
  return f->func(args);
}

//------------------------evaluator----------------------------
// Special form types
typedef enum { SF_NONE, SF_IF, SF_DEF, SF_QUOTE } special_form;

// Identify if list head is a spec. form
special_form get_special_form(pval* list) {
  res_array* d = (res_array*)list->data;
  if (d->size == 0 || d->items[0]->type != PVAL_SYMBOL)
    return SF_NONE;
  char* head = (char*)d->items[0]->data;
  if (strcmp(head, "if") == 0)
    return SF_IF;
  else if (strcmp(head, "def") == 0)
    return SF_DEF;
  else if (strcmp(head, "quote") == 0)
    return SF_QUOTE;
  return SF_NONE;
}

// Main evaluator 
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
    // Empty list handling
    if (d->size == 0)
      return pval_copy(pv);

    // Special form handling
    special_form sf = get_special_form(pv);
    if (sf != SF_NONE) {
      switch (sf) {
      case SF_IF:
        return eval_if(pv, e);
      case SF_DEF:
        return eval_def(pv, e);
      case SF_QUOTE:
        return eval_quote(pv, e);
      default:
        break;
      }
    }

    // Evaluate all elements
    pval* args = psi_eval_list(pv, e);
    if (args->type == PVAL_ERROR)
      return args;

    // Split function from args
    pval* func = pval_copy(((res_array*)args->data)->items[0]);
    pval* args1 = pval_list();
    for (int i = 1; i < ((res_array*)args->data)->size; i++) {
      pval_list_add(args1, pval_copy(((res_array*)args->data)->items[i]));
    }
    pval_delete(args);

    // Apply and return
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
// Main interpreter  
void psi_repl(env* e) {
  while (1) {
    char input[4096];
    fflush(stdout);
    // printf("debug: parsing\n");
    if (!fgets(input, 4096, stdin))
      break;
    printf("psi> %s", input);
    int idx = 0;
    pval* pv = pval_parse(input, &idx);
    // printf("debug: parsed\n");
    if (!pv)
      continue;
    // printf("debug: evaluating\n");
    pval* result = psi_eval(pv, e);
    // printf("debug: evaluated\n");
    pval_print(result);
    printf("\n\n");
    pval_delete(pv);
    pval_delete(result);
  }
}

//---------------------special forms---------------------------
// Evaluate condition / branch
pval* eval_if(pval* list, env* e) {
  res_array* d = (res_array*)list->data;
  if (d->size < 3 || d->size > 4)
    return pval_error(pval_symbol("arity-error"));
  pval* cond = psi_eval(d->items[1], e);
  if (cond->type == PVAL_ERROR)
    return cond;
  // PSI Spec: only #f is falsy; all other values are truthy
  bool is_false = (cond->type == PVAL_BOOL && !*((bool*)cond->data));
  pval_delete(cond);
  if (is_false)
    // PSI Spec: "In the arity-2 case, behaves as if else-form were given as
    // #f."
    return (d->size == 4) ? psi_eval(d->items[3], e) : pval_bool(false);
  return psi_eval(d->items[2], e);
}

// Evaluate expression, bind result to glob env
pval* eval_def(pval* list, env* e) {
  res_array* d = (res_array*)list->data;
  if (d->size != 3)
    return pval_error(pval_symbol("arity-error"));
  if (d->items[1]->type != PVAL_SYMBOL)
    return pval_error(pval_symbol("type-error"));
  pval* val = psi_eval(d->items[2], e);
  if (val->type == PVAL_ERROR)
    return val;
  env_bind(e, d->items[1], val);
  return val;
}

// Return unevaluated argument  
pval* eval_quote(pval* list, env* e) {
  res_array* d = (res_array*)list->data;
  if (d->size != 2)
    return pval_error(pval_symbol("arity-error"));
  return pval_copy(d->items[1]);
}

//-------------------------- main -----------------------------
// Global builtin functions loaded on startup
fpval global_env[] = {
    {"+", builtin_add},     {"-", builtin_sub},     {"*", builtin_mul},
    {"/", builtin_div},     {"=", builtin_eq},      {"quit", builtin_quit},
    {"cons", builtin_cons}, {"head", builtin_head}, {"tail", builtin_tail},
    {"type", builtin_type},
};
int global_env_size = 10;

// Entrypoint
int main() {
  env* e = env_init(global_env, global_env_size);
  psi_repl(e);
}