#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PAGE_SIZE 256 
#define FRAME_SIZE 256
#define PHY_MEM_SIZE 128 * FRAME_SIZE   // memory size = number of frames * frame size (32,768)
#define PAGE_TABLE_SIZE 256             // number of entries in page table
#define TLB_SIZE 16                     // number of entries in TLB
#define ADDRESS_SIZE 10
#define OFFSET_MASKER 0xFF
#define PAGE_MASKER 0xFFFF

char physical_memory[PHY_MEM_SIZE]; 
int page_table[PAGE_TABLE_SIZE]; 
int TLB[TLB_SIZE][2]; 
char backing_store_array[PAGE_SIZE]; 
char address[ADDRESS_SIZE];

int LRU_index(int history[], int size, int min)         //determines which page has been used the least
{
    int index = 0;

    if (size == TLB_SIZE)               // for TLB
    { 
        for(int i = 0; i < size; i++)   // search the history array to see if there's any lesser used element
        { 
            if(history[i] < min)
            {
                min = history[i];
                index = i;
            }
        }
    }
    else                                // for page table
    { 
        for(int i = 0; i < size; i++)
        {                               // search the history array to see if there's any element less than min (lesser used)
            if(history[i] < min && history[i] != 0)
            {   
                min = history[i];
                index = i;
            }
        }
    }

    return index;                       // returns the index of the TLB/page table to be replaced
}


int main(int argc, char *argv[]) 
{
    char* filename_in = argv[1]; 
    char* filename_out = "physicaladdresses.txt"; 
    char value;                         // value stored at the physical address

    int logical_address, physical_address, page_number, frame_number, offset;
    
    int memory_full = 0;
    int TLB_index = 0;                  // for LRU replacement selection
    int TLB_hit_counter = 0;
    int page_fault_counter = 0;
    int addresses_translated = 0;
    int memory_index = 0;
    int TLB_history[TLB_SIZE] = {};     // history array

    int page_table_existance[PAGE_TABLE_SIZE] = {}; 
    /*
    this array indicates if a frame exists in the page_table, and the valid bits of the entries 
    and also this array keeps track of history of the addresses each page 
    has been used (using this history implementing LRU page replacment policy)
    */

    if (filename_in == NULL) 
    { 
        printf("\nERROR!!       Enter input file name.\n");
        exit(1);
    } 
    else 
    {        
        FILE* input_file = fopen(filename_in, "r");
        if ( input_file == NULL) 
        {
            printf("ERROR!!     Input file does not exist.\n");
            exit(1);
        }
        
        FILE* output_file = fopen(filename_out, "w");
        if ( output_file == NULL) 
        {
            printf("ERROR!!     Output file could not be opened.\n");
            exit(1);
        }
        
        FILE* backing_store_file = fopen("BACKING_STORE.bin", "rb"); 
        if ( backing_store_file == NULL) 
        {
            printf("ERROR!!     BACKING_STORE.bin file could not be opened.\n");
            exit(1);
        }
    

        while (fgets(address,ADDRESS_SIZE, input_file))     // translating all the logical addresses into physical addresses
        {
            logical_address = atoi(address);
            addresses_translated++; 

            page_number = ((logical_address & PAGE_MASKER) >> 8);
            offset = (logical_address & OFFSET_MASKER);
                   
            // using page number we search for the frame number in TLB 
            int TLB_found = 0;
            for (int i = 0; i< TLB_SIZE; i++) 
            {
                if (TLB[i][0] == page_number)               // the page is in TLB
                { 
                    TLB_history[TLB_index] = addresses_translated;
                    TLB_hit_counter++;
                    frame_number = TLB[i][1]; 
                    TLB_found = 1;
                    break;
                }
            }
            
            if (TLB_found)                                  // TLB hit 
            { 
                page_table_existance[page_number] = addresses_translated; // the last time we use this page is the current address counter

                physical_address = frame_number + offset; 
                value = physical_memory[physical_address]; // get the value stored in the physical address
            } 
            else                                            // TLB miss
            {                                               // searching for the page number in the page table 
                
                if (page_table_existance[page_number] != 0) //page number found in the table , page is valid 
                { 
                    frame_number = page_table[page_number];
                    
                    page_table_existance[page_number] = addresses_translated;
                    
                    physical_address = frame_number + offset; 
                    value = physical_memory[physical_address]; 
                    
                    // updating the TLB with the page number and the page frame we got
                    TLB[TLB_index][0] = page_number;
                    TLB[TLB_index][1] = frame_number;
                    TLB_history[TLB_index] = addresses_translated; 
                    TLB_index = LRU_index(TLB_history,TLB_SIZE,addresses_translated); // getting the next TLB index to be replaced using LRU policy
                } 
                else                                        //page number not found in the page table , page fault
                {   
                    page_fault_counter++; 

                    if (memory_full == 0)                      // empty frame exists in memory
                    { 
                        /*  we read a page from the BACKING_STORE.bin file
                            and store it in an empty frame in the physical memory. 
                        */

                        fseek(backing_store_file,page_number*PAGE_SIZE,SEEK_SET); 
                        fread(backing_store_array,1,PAGE_SIZE,backing_store_file); 
                        memcpy(physical_memory + memory_index, backing_store_array, PAGE_SIZE); // copy backing_store_array into physical memory at the memory_index frame
                        
                        frame_number = memory_index;                // the frame number in the memory is the index of the memory
                        physical_address = frame_number + offset;
                        value = physical_memory[physical_address]; 
                        
                        //Updating page table
                        page_table[page_number] = frame_number;
                        page_table_existance[page_number] = addresses_translated; 
                        
                        //Updating TLB
                        TLB[TLB_index][0] = page_number;
                        TLB[TLB_index][1] = frame_number;
                        TLB_history[TLB_index] = addresses_translated;
                        TLB_index = LRU_index(TLB_history,TLB_SIZE,addresses_translated);
                        
                        if (memory_index < PHY_MEM_SIZE - FRAME_SIZE)       //memory is not full
                            memory_index += FRAME_SIZE;
                        else                                                // memory is full                       
                            memory_full = 1; 
                        
                    } 
                    else                                                    //memory full, no empty frames
                    { 
                        // using LRU Policy for page replacement       
                                          
                        int page_replace = LRU_index(page_table_existance,PAGE_SIZE,addresses_translated); // the selected page to be replaced (the least recently used one)

                        int selected_frame_number = page_table[page_replace]; //frame freed in physical memory
                                                
                        page_table_existance[page_replace] = 0; // not valid anymore because the corresponding frame has been replaced in memory

                        //reading page from the BACKING_STORE.bin file, stored in selected_frame_number
                        fseek(backing_store_file,page_number*PAGE_SIZE,SEEK_SET); 
                        fread(backing_store_array,1,PAGE_SIZE,backing_store_file); 
                        memcpy(physical_memory + selected_frame_number, backing_store_array, PAGE_SIZE); 

                        physical_address = selected_frame_number + offset; 
                        value = physical_memory[physical_address]; 
                    
                        //updating page table
                        page_table[page_number] = selected_frame_number;
                        page_table_existance[page_number] = addresses_translated; 

                        //updating TLB
                        TLB[TLB_index][0] = page_number;
                        TLB[TLB_index][1] = selected_frame_number;
                        TLB_history[TLB_index] = addresses_translated; 
                        TLB_index = LRU_index(TLB_history,TLB_SIZE,addresses_translated); // getting the next TLB index to be replaced  
                    }
                }
            }
            fprintf(output_file, "Virtual address: %d Physical address: %d Value: %d\n", logical_address,physical_address,value);
        }
        //end of input file
        
        fclose(input_file);
        fclose(output_file);
        fclose(backing_store_file);

        float page_fault_rate = (float) page_fault_counter / (float) addresses_translated;
        float TLB_rate = (float) TLB_hit_counter / (float) addresses_translated;

        printf("    TLB Hits : %d\n", TLB_hit_counter);
        printf("    TLB Hit Rate : %.4f%%\n", TLB_rate*100);
        printf("    Page Faults : %d\n", page_fault_counter);
        printf("    Page Fault Rate : %.4f%%\n", page_fault_rate*100);
        printf("    Total number of addresses translated: %d\n", addresses_translated);
    }   
    return 0;
}