#include <stdint.h>
#include <stdlib.h>

typedef void (*node_function)(void** inputs, void** outputs);

typedef struct node {
  uint64_t input0;  //index of first external input  pin
  uint64_t inputM;  //index of last  external input  pin (plus one)
  uint64_t output0; //index of first external output pin
  uint64_t outputN; //index of last  external output pin (plus one)
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

typedef struct circuit {
  void* pins[2]; //double buffer
  struct node n;
} circuit_t;

circuit_t* empty_circuit(void) {
  return calloc(1,sizeof(circuit_t));
}

node_t* constant_node(node_t* parent, void* output_constant, char* node_name) {
  parent->subnodes=realloc(parent->subnodes,sizeof(node_t)*++(parent->k));
  node_t* result = &parent->subnodes[parent->k-1];
  result->input0=0;
  result->inputM=0;
  result->output0=0;
  result->outputN=1;
  result->node_name = node_name;
  result->input_names  = NULL;
  result->output_names = {"constant"};
  result->function = NULL;
  result->output_constant = output_constant;
  result->subnodes = NULL;
  result->k = 0;
  result->ncxn = 0;
  result->cxn = NULL;
  return result;
}

