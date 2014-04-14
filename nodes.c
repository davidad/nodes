#include <stdlib.h>
#include <stdio.h>

double nonlinearity(double x) {
  return x >= 0;
}

struct node {
  int n_inputs;
  struct node* inputs;
  double* factors;
  double affine_term;
  double output;
};
typedef struct node *node_p;

typedef enum {
  AND,
  OR
} gate_type;

node_p make_node(gate_type t, int n_inputs, node_p inputs) {
  node_p result = malloc(sizeof(struct node));
  result->n_inputs = n_inputs;
  result->inputs = inputs;
  result->factors = malloc(n_inputs*sizeof(double));
  switch(t) {
    case AND:
      result->affine_term = -(n_inputs);
      break;

    case OR:
      result->affine_term = -1.0;
      break;
  };
  int i;
  for(i=0;i<n_inputs;i++) {
    result->factors[i] = 1.0;
  }
  result->output = 0.0;
  return result;
}

double run_node(node_p n) {
  double accumulator=n->affine_term;
  int i;
  for(i=0;i<n->n_inputs;i++) {
    accumulator += (n->inputs[i].output)*(n->factors[i]);
  }
  n->output = nonlinearity(accumulator);
  return n->output;
}

int main(int argc, char* argv[]) {
  if(argc<3) printf("Usage: %s <input 1> <input 2>\n",argv[0]), exit(1);
  struct node inputs[] = {
    {.output = strtod(argv[1],NULL)},
    {.output = strtod(argv[2],NULL)}
  };
  node_p and_node = make_node(AND,2,inputs);
  run_node(and_node);
  printf("The output is %lf\n",and_node->output);
  return 0;
}
