#define _GNU_SOURCE
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef void (*node_function)(void* inputs, void* outputs);

typedef struct node {
  uint64_t lg2_pin_size;
  void* pins;
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

node_t* empty_circuit(uint64_t pin_size, uint64_t input_pins, uint64_t output_pins) {
  node_t* circuit = calloc(1,sizeof(node_t));
  if(pin_size==0) pin_size++;
  circuit->lg2_pin_size = 64-__builtin_clzll(pin_size-1);
  pin_size = 1<<(circuit->lg2_pin_size);
  circuit->input0 = 0;
  circuit->inputM = input_pins;
  circuit->output0 = input_pins;
  circuit->outputN = input_pins+output_pins;
  circuit->pins = calloc(pin_size,input_pins+output_pins);
  return circuit;
}

int add_pins(node_t* parent, uint64_t n_pins) {
  if(!parent) return 0;
  int i; node_t** pp;
  if(parent->n_parents > 0) {
    for(i=0,pp=*(parent->parents); i<parent->n_parents; i++,pp++) {
      // v this condition checks that we're adding to a youngest sibling
      if((*pp)->output0 - (*pp)->input0 != parent->outputN || !add_pins(*pp,n_pins)) return 0;
      // TODO actually handle non-youngest-sibling case, instead of bailing
    }
  }
  parent->output0 += n_pins;
  parent->outputN += n_pins;
  if(parent->n_parents == 0) {
    parent->pins = realloc(parent->pins,(1<<parent->lg2_pin_size)*(parent->outputN));
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
    result->lg2_pin_size = parent->lg2_pin_size;
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
    result->input0  = offs;        offs-=n->input0;
    result->inputM  = n->inputM  + offs;
    result->output0 = n->output0 + offs;
    result->outputN = n->outputN + offs;
  }
  return result;
}

int connect_by_names(node_t* p, char* from_name, uint64_t out_pin, char* to_name, uint64_t in_pin) {
  uint64_t from_pin, to_pin;
  int i;

  //TODO: use tsearch here
  if(!from_name) {
    from_pin=p->input0+out_pin;
    if(from_pin<p->inputM);
      else return 1 | 1<<2;
  } else {
    for(i=0;i<p->k;i++) if(!strcmp(p->subnodes[i].node_name,from_name)) break;
    if(i<p->k) from_pin = p->subnodes[i].output0+out_pin;
      else return 2 | 1<<2;
  }
  if(!to_name) {
    to_pin=p->output0+in_pin;
    if(to_pin<p->outputN);
      else return 1 | 2<<2;
  } else {
    for(i=0;i<p->k;i++) if(!strcmp(p->subnodes[i].node_name,to_name)) break;
    if(i<p->k) to_pin = p->subnodes[i].output0+in_pin;
      else return 2 | 2<<2;
  }

  p->cxn = realloc(p->cxn, ++p->ncxn);
  p->cxn[p->ncxn-1].from_pin = from_pin;
  p->cxn[p->ncxn-1].to_pin = to_pin;
  return 0;
}

int run_node(node_t* n, uint64_t lg2_pin_size, uint64_t pin_offs, void* pins) {
#define PIN(x) ((((x)+pin_offs)<<lg2_pin_size)+pins)
  const uint64_t pin_size = 1<<lg2_pin_size;
  int i;
  if(n->output_constant) {
    for(i=n->output0;i<n->outputN;i++) {
      memcpy(PIN(i),n->output_constant,pin_size);
    }
    return 0;
  } else if (n->function) {
    void* inputs  = PIN(n->input0);
    void* outputs = PIN(n->output0);
    n->function(inputs,outputs);
  } else if (n->subnodes) {
    pin_offs+=n->input0;
    node_t* subnode;
    for(i=0, subnode=n->subnodes; i<n->k; i++, subnode++) {
      run_node(subnode,lg2_pin_size,pin_offs,pins);
    }
    struct connection* cxn;
    for(i=0, cxn=n->cxn; i<n->ncxn; i++, cxn++)  {
      memcpy(PIN(cxn->to_pin),PIN(cxn->from_pin),pin_size);
    }
  } else {
    return -2;
  }
#undef PIN
}

int run_circuit(node_t* n) {
  return run_node(n,n->lg2_pin_size,0,n->pins);
}

inline void* get_output(node_t* n, uint64_t out_pin) {
  return ((n->output0+out_pin)<<n->lg2_pin_size)+n->pins;
}

inline void set_input(node_t* n, uint64_t in_pin, const void* value) {
  uint64_t l2ps = n->lg2_pin_size;
  memcpy(((n->input0+in_pin)<<l2ps)+n->pins,value,1<<l2ps);
}

typedef struct { char* s; node_t n; } symbol_t;
static symbol_t stdlib[];
void load_stdlib(void) {
  stdlib = {
    {"and_node", NULL},
    {"or_node",  NULL}
  };
}

void parse_circuit(char* filename, size_t pin_size) {
  FILE* f = fopen(filename,"r"); if(f==NULL) perror("Error opening circuit file");

  /* Parse header block-- example_subnode: A, B -> C, D */
  char* node_name[1], input_names[256], output_names[256];
  if(fscanf(f,"%ms: ",node_name)!=1) syntax_error("Missing colon?");
  int n_inputs=0, n_outputs=0;
  do {
    if(fscanf(f,"%m[^-, ]s",input_names+(n_inputs++))!=1) break;
  } while(fgetc(f)==',');
  fscanf(f,"->");
  do {
    if(fscanf(f,"%m[^, ]s",output_names+(n_outputs++))!=1) break;
  } while(fgetc(f)==',');
  node_t* circuit = empty_circuit(pin_size,n_inputs,n_outputs);
  circuit->node_name = *node_name;

  /* Parse subnodes-- or1 <= or_node */
  while(fscanf(f,"%ms <=",node_name)==1) {
    char* prototype[1];
    fscanf(f,"%ms",prototype);

  }
}

int main() {
  node_t* circuit = empty_circuit(sizeof(double),0,1);
  double constant_one = 1.0;
  insert_constant_node(circuit,(void*)&constant_one,"constant one");
  connect_by_names(circuit,"constant one",0,NULL,0);
  run_circuit(circuit);
  printf("output: %lf\n",*(double*)get_output(circuit,0));
  return 0;
}
