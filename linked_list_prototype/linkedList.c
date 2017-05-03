#include "linkedList.h"
#include <stdlib.h>
#include <string.h> 
#include <stdio.h>


LinkedList* LLcreate(void){
	LinkedList * newList;
	newList = malloc(sizeof(LinkedList));
	newList->prev = NULL;
	newList->curr = newList;
	newList->next = NULL;
	newList->name = (char * )"First";
	return newList;
}


LinkedList* Next(LinkedList* list){
	return list->next;
}
char* Curr(LinkedList* list){
	return list->name;
}
LinkedList* Prev(LinkedList* list){
	return list->prev;
}


void addWithDatum(char * name, void * dataum, LinkedList* list){
	list->next = LLcreate();
	list->next->name = name;
	list->next->prev = list;
	list->next->data = dataum;
}
void add(char * name, LinkedList* list){
	list->next = LLcreate();
	list->next->name = name;
	list->next->prev = list;
}

void removeNode(LinkedList* list){
	if(list->prev){
	list->prev->next = list->next;
	}
	if(list->next){

	list->next->prev = list->prev;
	}
	LLdestroy(list);
}


void LLdestroy(LinkedList* boom){
	boom->prev = NULL;
 	boom->curr = NULL;
 	boom->next = NULL;

	free(boom);
	boom = NULL;
}


int main () {
	LinkedList * list = LLcreate();
	add((char*)"second", list);
	add((char*)"third", list->next);
	add((char*)"fourth", list->next->next);
	removeNode(list->next->next);

	printf("%s\n",list->name);
	printf("%s\n",list->next->name);
	printf("%s\n",list->next->next->name);

	return 0;
} 
