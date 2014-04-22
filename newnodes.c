#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef void (*node_function)(void** inputs, void** outputs);

typedef struct node {
  size_t pin_size;
  void* pins[2]; //double buffer. NULL unless this is the top-level circuit
  uint64_t n_parents;
  struct node*** parents;
  uint64_t input0;  //index of first external input  pin w.r.t. parent
  uint64_t inputM;  //index of last  external input  pin " " (plus one)
  uint64_t output0; //index of first external output pin w.r.t. parent
  uint64_t outputN; //index of last  external output pin " " (plus one)
  char* node_name;
  char** input_names;
  char** output_names;
  node_function function;     // Only one of
  void* output_constant;      // these should be
  struct node* subnodes;      // non-NULL.
  uint64_t k;             // number of subnodes
  uint64_t ncxn;          // number of internal connections
  struct connection {
    uint64_t from_pin;
    uint64_t to_pin;
  } *cxn;
} node_t;

node_t* empty_circuit(size_t pin_size) {
  node_t* circuit = calloc(1,sizeof(node_t));
  circuit->pin_size = pin_size;
}

int add_pins(node_t* parent, uint64_t n_pins) {
  if(!parent) return 0;
  int i; node_t** pp;
  for(i=0,pp=*(parent->parents); i<parent->n_parents; i++,pp++) {
    // v this condition checks that we're adding to a youngest sibling
    if((*pp)->output0 - (*pp)->input0 != parent->outputN || !add_pins(*pp,n_pins)) return 0;
    // TODO actually handle non-youngest-sibling case, instead of bailing
  }
  parent->output0 += n_pins;
  parent->outputN += n_pins;
  if(parent->n_parents == 0) {
    for(i=0;i<=1;i++)
      parent->pins[i] =
        realloc(parent->pins[i],(parent->pin_size)*(parent->outputN));
  }
  return 1;
}

node_t* insert_new_node(node_t* parent, uint64_t input_pins, uint64_t output_pins) {
  if(!parent) return NULL;
  uint64_t n_pins = input_pins + output_pins;
  node_t* result = NULL;
  uint64_t offs = parent->output0 - parent->input0;
  if(add_pins(parent,n_pins)) {
    parent->subnodes=realloc(parent->subnodes,sizeof(node_t)*++(parent->k));
    result = &parent->subnodes[parent->k-1];
    result->input0 = offs;
    result->inputM = offs + input_pins;
    result->output0 = result->inputM;
    result->outputN = result->inputM + output_pins;
    node_t** parents = malloc(sizeof(node_t*));
    result->parents = malloc(sizeof(node_t**));
    *(result->parents) = parents;
    parents[0]=parent;
    result->n_parents=1;
  }
  return result;
}

static char* constant_output_names[] = {"constant"};
node_t* insert_constant_node(node_t* parent, void* output_constant, char* node_name) {
  node_t* result = insert_new_node(parent,0,1);
  if(result) {
    result->output_constant = output_constant;
    result->output_names = constant_output_names;
    result->node_name = node_name;
    result->input_names  = NULL;
    result->function = NULL;
    result->subnodes = NULL;
    result->k = 0;
    result->ncxn = 0;
    result->cxn = NULL;
  }
  return result;
}

node_t* insert_function_node(node_t* parent, node_function f, uint64_t m, uint64_t n, char** input_names, char** output_names, char* node_name) {
  node_t* result = insert_new_node(parent,m,n);
  if(result) {
    result->function = f;
    result->input_names = input_names;
    result->output_names = output_names;
    result->node_name = node_name;
    result->subnodes = NULL;
    result->k = 0;
    result->ncxn = 0;
    result->cxn = NULL;
  }
  return result;
}

node_t* copy_node(node_t* parent, node_t* n) {
  uint64_t n_pins = n->outputN - n->input0;
  uint64_t offs = parent->output0 - parent->input0;
  node_t* result = NULL;
  if(add_pins(parent,n_pins)) {
    if(!n->parents) n->parents=calloc(1,sizeof(node_t**));
    *(n->parents) = realloc(*(n->parents),sizeof(node_t*)*++(n->n_parents));
    (*(n->parents))[n->n_parents-1] = parent;
    parent->subnodes = realloc(parent->subnodes,sizeof(node_t)*++(parent->k));
    result = &parent->subnodes[parent->k-1];
    memcpy(result, n, sizeof(node_t));
    result->input0 = offs;
    result->inputM = offs - n->input0 + n->inputM;
    result->output0 = offs - n->input0 + n->output0;
    result->outputN = offs - n->input0 + n->outputN;
  }
  return result;
}

int main() {
  return 0;
}
