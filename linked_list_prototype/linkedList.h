#include <stdbool.h>
typedef struct LinkedList {
 	struct LinkedList* prev;
 	struct LinkedList* curr;
 	struct LinkedList* next;

 	bool alloced;

 	char * name;
	void * data;
} LinkedList;


LinkedList* Next(LinkedList* list);
char* Curr(LinkedList* list);
LinkedList* Prev(LinkedList* list);

void addWithDatum(char * name, void * dataum, LinkedList* list);
void add(char * name, LinkedList* list);

void removeNode(LinkedList* list);

LinkedList* LLcreate();
void LLdestroy(LinkedList* boom);
