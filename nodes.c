#include <stdlib.h>
#include <stdio.h>

double nonlinearity(double x) {
  return x >= 0;
}

struct node {
  int n_inputs;
  double affine_term;
  double output;
  struct connection {
    double factor;
    struct node* input;
  } connections[];
};
typedef struct node *node_p;

typedef enum {
  AND,
  OR
} gate_type;

node_p make_node(gate_type t, int n_inputs, node_p* inputs) {
  node_p result = malloc(sizeof(struct node)+n_inputs*sizeof(struct connection));
  result->n_inputs = n_inputs;

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
    result->connections[i].factor = 1.0;
    result->connections[i].input  = inputs[i];
  }
  result->output = 0.0;
  return result;
}

double run_node(node_p n) {
  double accumulator=n->affine_term;
  int i;
  struct connection* cp = n->connections;
  for(i=0;i<n->n_inputs;i++,cp++) {
    accumulator += (cp->input->output)*(cp->factor);
  }
  n->output = nonlinearity(accumulator);
  return n->output;
}

int main(int argc, char* argv[]) {
  if(argc<4) printf("Usage: %s <input 1> <input 2>\n",argv[0]), exit(1);
  struct node inputs[] = {
    {.output = strtod(argv[1],NULL)},
    {.output = strtod(argv[2],NULL)},
    {.output = strtod(argv[3],NULL)}
  };
/*
 * input[0] ->  | and_node1 ->                               |
 * input[1] ->  |            | and_node2 ->                  | or_node1 ->
 * input[2] ->               |             | and_node3 ->    |
 * input[0] ->                             |
 */
  node_p input_ps[] = { &inputs[0], &inputs[1], &inputs[2], &inputs[0] };
  node_p and_node1 = make_node(AND,2,input_ps);
  node_p and_node2 = make_node(AND,2,input_ps+1);
  node_p and_node3 = make_node(AND,2,input_ps+2);
  node_p layer1_ps[] = { and_node1, and_node2, and_node3 };
  node_p or_node1 = make_node(OR,3,layer1_ps);
  node_p nodes_to_run[] = { and_node1, and_node2, and_node3, or_node1 };
  for(int i=0;i<sizeof(nodes_to_run)/sizeof(node_p);i++) {
    run_node(nodes_to_run[i]);
  }
  printf("The output is %lf\n",or_node1->output);
  return 0;
}
