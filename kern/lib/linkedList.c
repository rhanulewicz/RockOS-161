#include "linkedList.h"
#include <types.h>
#include <lib.h>
// #include <stdlib.h>
// #include <string.h> 
// #include <stdio.h>


LinkedList* LLcreate(){
	LinkedList * newList;
	newList = kmalloc(sizeof(LinkedList));
	newList->prev = NULL;
	newList->curr = newList;
	newList->next = NULL;
	newList->name = (char * )"First";
	return newList;
}

LinkedList* LLcreateWithName(char * name){
	LinkedList * newList;
	newList = kmalloc(sizeof(LinkedList));
	newList->prev = NULL;
	newList->curr = newList;
	newList->next = NULL;
	newList->name = name;
	return newList;
}


LinkedList* LLnext(LinkedList* list){
	return list->next;
}
char* LLcurr(LinkedList* list){
	return list->name;
}
LinkedList* LLprev(LinkedList* list){
	return list->prev;
}


void LLaddWithDatum(char * name, void * dataum, LinkedList* list){
	list->next = LLcreateWithName(name);
	list->next->name = name;
	list->next->prev = list;
	list->next->data = dataum;
}
void LLadd(char * name, LinkedList* list){
	list->next = LLcreateWithName(name);
	list->next->name = name;
	list->next->prev = list;
}	

void LLremoveNode(LinkedList* list){
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

	kfree(boom);
	boom = NULL;
}


// int main () {
// 	LinkedList * list = LLcreate();
// 	add((char*)"second", list);
// 	add((char*)"third", list->next);
// 	add((char*)"fourth", list->next->next);
// 	removeNode(list->next->next);

// 	printf("%s\n",list->name);
// 	printf("%s\n",list->next->name);
// 	printf("%s\n",list->next->next->name);

// 	return 0;
// } 
