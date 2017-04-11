
typedef struct LinkedList {
 	struct LinkedList* prev;
 	struct LinkedList* curr;
 	struct LinkedList* next;

 	// alloced;

 	char * name;
	void * data;
} LinkedList;


LinkedList* LLnext(LinkedList* list);
char* LLcurr(LinkedList* list);
LinkedList* LLprev(LinkedList* list);

void LLaddWithDatum(char * name, void * dataum, LinkedList* list);
void LLadd(char * name, LinkedList* list);

void LLremoveNode(LinkedList* list);

LinkedList* LLcreate(void);
LinkedList* LLcreateWithName(char * name);

void LLdestroy(LinkedList* boom);
