#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#define LEN sizeof(struct students)
enum Xml_Name {
    filename,
    rootnode,
    childnode,
    node_prop_Name,
    node_prop_Number,
    node_prop_Age,
    node_prop_Country
};
const char *g_str[] = {
    "my.xml",
    "root",
    "student",
    "Name",
    "Number",
    "Age",
    "Country"
};
struct students
{  
    xmlChar *name;
    xmlChar *number;
    xmlChar *age;
    xmlChar *country;
    struct students * next;
};
/*********************According to the node print content*********************/
int print_node (struct students * head)
{   
    struct students * ptr_print = NULL;
    
    if ( head == NULL )
    { 
        printf("Parameters Transfer error\n");
        return  0;
    }   
    
    ptr_print = head;
    printf("Name    \tNumber\tAge\tcountry\n");
    
    while (ptr_print != NULL)
    {  
        printf("%s    \t%s\t%s\t%s\n",ptr_print->name,ptr_print->number,ptr_print->age,ptr_print->country);
        ptr_print = ptr_print->next;
    }
    return 1;    
} 
/*******************In alphabetical order print content*******************/
void print_alphabet(struct students * head)      
{   
    int compare; 
    struct students * max;
    struct students * min;
    struct students st;
    
    if ( head == NULL )
    { 
       printf("Parameters Transfer error\n");
       return  ;
    }   
    min = head;
    max = head;
     
    while (min != NULL)
    {  
        max = min->next;
        while (max != NULL)                        
        {         
            compare = xmlStrcmp(min->name, max->name);
            //printf(" %d !!!!!!\n",compare);
            if ( compare > 0)
            { 
                st.name    = min->name;
                st.number  = min->number;
                st.age     = min->age;
                st.country = min->country;
                        
                min->name    = max->name;
                min->number  = max->number;
                min->age     = max->age;
                min->country = max->country;
                        
                max->name    = st.name;
                max->number  = st.number;
                max->age     = st.age;
                max->country = st.country;
            }  
            else  ;
            max = max->next;
        }
        min = min->next;     
    }  
    printf("\n");
    printf("        In alphabetical order print content!\n");  
} 
/*******************free the linked list*******************/
void free_link(struct students * head)
{   
    struct students * ptr;
    struct students * ptr_free;
    
    if ( head == NULL )
    { 
       printf("Parameters Transfer error\n");
       return  ;
    }   
    ptr_free = head;
    ptr = head;
    
    while (ptr != NULL)
    {  
        ptr = ptr->next;
        xmlFree (ptr_free->name);
        xmlFree (ptr_free->number);
        xmlFree (ptr_free->age);
        xmlFree (ptr_free->country);
        xmlFree(ptr_free);
        ptr_free = ptr;
    }
}
/********************Analysis of XML content stored in the linked list*************/
struct students * parse_link(xmlDocPtr Ptr_doc, xmlNodePtr curNode)
{
    xmlChar *key = NULL;
        
    if ( Ptr_doc == NULL && curNode == NULL)
    { 
        printf("Parameters Transfer error\n");
        return  0;
    }   
             
    curNode = curNode->xmlChildrenNode;
    struct students *head = NULL;
    struct students *ptr_insit_node = NULL;
    struct students *ptr_create_node = NULL;
    head = ptr_insit_node = ( struct students *) xmlMalloc (LEN);
    memset(ptr_insit_node,0,LEN);   
    curNode = curNode->next;
    
    if ((xmlStrcmp(curNode->name, (const xmlChar*)g_str[childnode])))
    {
        printf("this a error C process!\n");   
        return  0;
    }    
          
    while (curNode != NULL)
    {       
        key = xmlGetProp(curNode,BAD_CAST g_str[node_prop_Name]);
        ptr_insit_node->name = key ;
        key = NULL;
               
        key = xmlGetProp(curNode,BAD_CAST g_str[node_prop_Number]);
        ptr_insit_node->number = key ;
        key = NULL;
               
        key = xmlGetProp(curNode,BAD_CAST g_str[node_prop_Age]);
        ptr_insit_node->age = key ;
        key = NULL;
                
        key = xmlGetProp(curNode,BAD_CAST g_str[node_prop_Country]);
        ptr_insit_node->country = key ;
        key = NULL;
                
        curNode = curNode->next;
        curNode = curNode->next;
                
        if (curNode != NULL)
        {
            ptr_create_node = (struct students *) malloc (LEN);     
            memset(ptr_create_node,0,LEN);
            ptr_insit_node->next = ptr_create_node;
            ptr_insit_node = ptr_create_node;
        } 
                
    }
    printf("create link list OK!\n") ;
    printf("        According to the node print content !\n");
    return head;
}
/*********************main function*******************************/
int main()
{
    xmlDocPtr Ptr_doc;
    xmlNodePtr Ptr_cur;
    struct students * head = NULL;
     
    Ptr_doc = xmlParseFile(g_str[filename]);
    if (Ptr_doc == NULL)
    {
        printf("Document not parsed successfully. \n");
        exit(1);
    }
    printf("xmlParseFile ok.\n");
   
    Ptr_cur = xmlDocGetRootElement(Ptr_doc);
    if (Ptr_cur == NULL)
    {
        printf("empty document. \n");
        xmlFreeDoc(Ptr_doc);
        exit(1);
    }
    printf("xmlDocGetRootElement ok.\n");
    if (xmlStrcmp(Ptr_cur->name, (const xmlChar *) g_str[rootnode]))
    {
        printf("document of the wrong type, root node != root\n");
        xmlFreeDoc(Ptr_doc);
        exit(1);
    }

    head = parse_link(Ptr_doc, Ptr_cur);   
    print_node ( head );            
    print_alphabet(head);                
    print_node ( head );
        
    free_link(head);             
    xmlFreeDoc(Ptr_doc);               
    xmlCleanupParser();
    exit(0);
}
