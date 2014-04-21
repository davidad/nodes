#include <search.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

double nonlinearity(double x) {
  return x >= 0;
}

struct node {
  char* name;
  unsigned int n_inputs;
  double affine_term;
  double output;
  struct node* run_next;
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

node_p make_node(gate_type t, unsigned int n_inputs, node_p* inputs) {
  node_p result = malloc(sizeof(struct node)+n_inputs*sizeof(struct connection));
  result->n_inputs = n_inputs;

  switch(t) {
    case AND:
      result->affine_term = -(double)(n_inputs);
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
    printf("  %s <-- %lf --- %s\n",n->name,cp->input->output,cp->input->name);
    accumulator += (cp->input->output)*(cp->factor);
    printf("  accumulator: %lf\n",accumulator);
  }
  n->output = nonlinearity(accumulator);
  printf("node %s outputs %lf\n\n",n->name,n->output);
  return n->output;
}

void syntax_error(FILE* f,const char* msg) {
  char current_line[1024];
  if(fscanf(f,"%1024[^\n]",current_line)!=1) {
    do fseek(f,-1,SEEK_CUR); while (getc(f)!='\n' && !fseek(f,-1,SEEK_CUR));
    if(fscanf(f,"%1024[^\n]",current_line)!=1) fprintf(stderr,"Syntax error at position %ld: %s\n",ftell(f),msg), exit(1);
  }
  fprintf(stderr,"Syntax error at \"%s\": %s\n",current_line,msg), exit(1);
}

static int cmp_name(const void* a, const void* b) {
  return strcmp(((node_p)a)->name,((node_p)b)->name);
}

#define node_name_maxlen 256
#define node_name_fmt "%256[$0-9a-zA-Z_] "

int main(int argc, char* argv[]) {
  if(argc<2) fprintf(stderr,"Usage: %s <circuit file> [<input value> <input value> ...]\n",argv[0]), exit(1);
  FILE* f = fopen(argv[1],"r"); if(f==NULL) perror("Error opening circuit file");
  unsigned int n_inputs, i;
  void* name_lookup[1] = {NULL};

  node_p* global_inputs;
  { /* Parse inputs block, formatted like this:
     *   inputs: 3
     *     A B C
     * Create a "dummy" node for each input, and store in global_inputs.
     */
    if(fscanf(f,"inputs: %u ",&n_inputs)!=1) syntax_error(f,"expected number of inputs");
    global_inputs = malloc(sizeof(node_p)*n_inputs);
    for(i=0;i<n_inputs;i++) {
      node_p n = malloc(sizeof(struct node));
      n->name=malloc(node_name_maxlen);
      if(fscanf(f,node_name_fmt,n->name)!=1) syntax_error(f,"expected input name");
      realloc(n->name,strlen(n->name)+1);
      tsearch((void*)n,name_lookup,cmp_name);
      global_inputs[i]=n;
    }
  }

  node_p run_list_head=NULL, run_list_tail=NULL;
  { /* Parse the main node definitions, which are one line each, looking like this:
     *   & A B C -> and_node
     * Store these nodes into a linked list called run_list, in the order in which
     * they appear - that's the order in which they'll be run. (No cycles allowed!)
     */
    do {
      // Parse gate_type sigil
      char c;
      if(fscanf(f,"%1[&|] ",&c)!=1) break;
      gate_type t;
      switch(c) {
        case '&': t=AND; break;
        case '|': t=OR;  break;
      }

      // Parse input names
      char input_name[node_name_maxlen];
      unsigned int n_inputs = 0;
      node_p* inputs = NULL;
      while(fscanf(f,node_name_fmt,input_name)==1) {
        inputs = realloc(inputs,++n_inputs*sizeof(node_p));
        struct node n_search[1] = {{.name = input_name}};
        inputs[n_inputs-1] = *(node_p*)tfind((void*)n_search,name_lookup,cmp_name);
        if(!inputs[n_inputs-1]) fprintf(stderr,"no such node %s",input_name),exit(2);
      }
      
      // Make the node
      node_p n = make_node(t,n_inputs,inputs);
      free(inputs);

      // Give the node a name
      n->name=malloc(node_name_maxlen);
      if(fscanf(f,"-> " node_name_fmt,n->name)!=1) syntax_error(f,"expected node name");
      realloc(n->name,strlen(n->name)+1);
      tsearch((void*)n,name_lookup,cmp_name);
      
      // Attach node to run_list
      if(!run_list_head) run_list_head = n;
      if(run_list_tail) run_list_tail->run_next=n;
      run_list_tail=n;
    } while(!feof(f) && !ferror(f));
  }

  unsigned int n_outputs;
  node_p* outputs;
  { /* Parse outputs, formatted like the inputs, except with the word outputs:
     *   outputs:
     *     out_node1 out_node2
     * Store pointers to the output nodes in the outputs array.
     */
    if(fscanf(f,"outputs: %u ",&n_outputs)!=1) syntax_error(f,"expected number of outputs");
    outputs = malloc(n_outputs*sizeof(node_p));
    for(i=0;i<n_outputs;i++) {
      char output_name[node_name_maxlen];
      if(fscanf(f,node_name_fmt,output_name)!=1) syntax_error(f,"expected output name");
      struct node n_search[1] = {{.name=output_name}};
      outputs[i] = *(node_p*)tfind((void*)n_search,name_lookup,cmp_name);
      if(!outputs[i]) fprintf(stderr,"no such node %s",output_name),exit(2);
    }
  }
  fclose(f);

  { /* Actual execution of the parsed circuit happens here.
     */
    // Scan the inputs from the command line, and tell the corresponding dummy nodes what to output
    if(argc-2<n_inputs) fprintf(stderr,"Error: expected %d input values on the command line; got %d\n",n_inputs,argc-2),exit(3);
    for(i=0;i<n_inputs;i++) {
      global_inputs[i]->output=strtod(argv[i+2],NULL);
    }

    // Run all the internal nodes, traversing the run_list
    node_p node_to_run;
    for(node_to_run = run_list_head; node_to_run; node_to_run = node_to_run->run_next) {
      run_node(node_to_run);
    }

    // Print all the outputs
    for(i=0;i<n_outputs;i++) {
      printf("output \"%s\": %lf\n",outputs[i]->name,outputs[i]->output);
    }
  }
  return 0;
}
