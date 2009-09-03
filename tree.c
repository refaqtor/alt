/* alt - abstract language tree // pancake<at>nopcode.org */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "alt.h"

static inline AltNode* node_alloc(AltTree *at) {
	//return malloc(sizeof(AltNode)); // few times slower :D
	if (at->ncount >= ALLOC_POOL_SIZE) {
		if (++at->npool >= ALLOC_POOL_COUNT ) {
			printf("FAIL; Cannot allocate more memory in the pool\n");
			exit(1);
		}
		at->nodes[at->npool] = malloc(sizeof(AltNode)*ALLOC_POOL_SIZE);
		at->ncount = 0;
	}
	return &at->nodes[at->npool][at->ncount++];
}

static AltNode *alt_node_new(AltTree *at) {
	AltNode *node = node_alloc (at);
	memset (node, 0, sizeof (AltNode));
	return node;
}

static void engine_cb_level(AltState *st, int delta, char ch) {
	AltTree *at = (AltTree *) st->user;
	if (delta>0) at->depth[st->level] = at->cur;
	else at->cur = at->depth[st->level];
	if (st->debug) {
		printf ("%d ", st->level);
		PRINTLEVEL(st->level);
		if (delta>0) printf ("{\n");
		else printf("}\n");
	}
}

static int _word_type(AltState *st) {
	int type = TYPE_WORD;
	if (st->mode == MODE_STRING) {
		type = TYPE_STRING;
	} else
	if (*st->str>='0' && *st->str<='9') {
		if (strchr(st->str, '.'))
			type = TYPE_FLOAT;
		else type = TYPE_INTEGER;
	} else 
	if (parse_is_operator(*st->str))
		type = TYPE_OPERATOR;
	return type;
}

static void engine_cb_word(AltState *st) {
	AltTree *at = (AltTree *) st->user;
	AltNode *node;

	if (at->laststr && !*st->str && !*at->laststr)
		return;
	node = alt_node_new (st->user);

	node->type = _word_type (st);
	node->str = strdup (st->str);
	at->laststr = node->str;
	if (at->root == NULL)  /* define root node */
		at->root = at->depth[0] = at->cur = node;
	if (at->lastlevel < st->level) {
		/* set child */
		at->cur->right = node;
		node->left = at->cur;
		node->level = st->level;
		at->cur = node;
	} else {
		/* add node at same nest level */
		if (node != at->root) // do not infinite loop when only one node
			at->cur->down = node;
		node->up = at->cur;
		node->level = st->level;
		at->cur = node;
	} 
	if (st->debug) {
		printf ("%d ", st->level);
		PRINTLEVEL (st->level);
		printf ("'%s'\n", st->str);
	}
	at->lastlevel = st->level;
}

//const char *nodenamez[4] = {"word", "string", "int", "float"};
const char *nodetypez = "WSIFO";

static void _alt_tree_walk(AltNode *node) {
	if (node) {
		PRINTLEVEL (node->level);
		printf(" - %d %c : (%s)\n", node->level, nodetypez[node->type], node->str);
		_alt_tree_walk (node->right);
		_alt_tree_walk (node->down);
	}
}

AltNode *alt_tree_child(AltNode *node) {
	if (node == NULL)
		return NULL;
	if (node->right != NULL)
		return node->right;
	node = node->down;
	if (node && !*node->str)
		return node->right;
	return node;
}

void alt_tree_walk(AltState *st) {
	AltTree *at = (AltTree*) st->user;
	AltNode *node = at->root;
	printf ("WALK NODES:\n");
	while (node) {
		printf (" = %d %c : (%s)\n", node->level, nodetypez[node->type], node->str);
		_alt_tree_walk (node->right);
		node = node->down;
	}
}

AltNode* alt_tree_resolve(AltState *st, const char *name) {
	AltTree *at = (AltTree*) st->user;
	AltNode *node = at->root;
	while (node) {
		if (!strcmp (node->str, name))
			return node;
		node = node->down;
	}
	return NULL;
}

void alt_tree(AltState *st, int debug) {
	AltTree *at = (AltTree*) malloc (sizeof(AltTree));
	at->depth[0] = at->cur = at->root = 0;

	at->npool = 0;
	at->nodes[0] = malloc(sizeof(AltNode)*ALLOC_POOL_SIZE);
	at->ncount = 0;

	st->user = (void *) at;
	st->debug = debug;
	st->cb_word = engine_cb_word;
	st->cb_level = engine_cb_level;
}

void alt_tree_free(AltState *st) {
	int i;
	AltTree *at;
	if (st != NULL) {
		at = (AltTree*) st->user;
		for(i=0; i<at->npool; i++)
			free (at->nodes[i]);
		free(at);
		st->user = NULL;
	}
}
