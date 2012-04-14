#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <string.h>

#include "pin.H"

using namespace std;

typedef struct {
   ADDRINT sa; //stream starting address
   UINT32  sl; //stream length
   vector<int> insvalues; //array of Insval's, one for each instruction
   UINT32  scount; //stream count -- how many times it has been executed
   UINT32  lscount; //number of memory-referencing instructions
   UINT32  nstream; //number of unique next streams
   UINT32  img; //index into img_name_list
   UINT32  rtn; //index into rtn_name_list
   map<UINT32,UINT32> next_stream; //<stream index,times executed> count how many times the next stream is encountered
} stream_table_entry;

typedef pair<ADDRINT,UINT32> key; //<address of block,length of block>
typedef map<key,UINT32> stream_map; //<block key,index in stream_table>

enum Insval { INS_NORMAL, INS_READ, INS_WRITE };

static stream_map stream_ids; //maps block keys to their index in the stream_table
static vector<stream_table_entry*> stream_table; //one entry for each unique (by address & length) block

static UINT32 numStreamD = 0; //number of program streams executed (dynamic)
static UINT32 numMemRef = 0; //number of memory referencing instructions executed (dynamic)
static UINT32 numIrefs = 0; //number of instructions executed (dynamic)
static UINT32 maxStreamLen = 0; //max stream length (max # of instructions executed in sequence w/o branch)

static INT32 prev_stream_id = -1; //the previously executed stream's index in the stream_table

static vector<string> img_name_list;
static vector<string> rtn_name_list;

stream_table_entry* current_stream = new stream_table_entry;

//called whenever a branch is taken: store current_stream and start a new one
VOID branch_taken(ADDRINT sa)
{
   key k(current_stream->sa,current_stream->sl);
   stream_map::iterator loc = stream_ids.find(k);
   if(loc == stream_ids.end()) {
       stream_table.push_back(current_stream);
       stream_ids.insert(pair<key,UINT32>(k, stream_table.size()-1));
       loc = stream_ids.find(k);
       if(current_stream->sl > maxStreamLen) maxStreamLen = current_stream->sl;
   }
   //track number of times this stream was executed
   stream_table[loc->second]->scount++;

   //add an entry to the previous stream's next_stream map
   if(prev_stream_id >= 0) {
       stream_table_entry* prev_stream = stream_table[prev_stream_id];
       if(prev_stream->next_stream.find(loc->second) != prev_stream->next_stream.end()) {
           prev_stream->next_stream[loc->second]++;
       }
       else {
           prev_stream->next_stream.insert(pair<UINT32,UINT32>(loc->second,1));
       }
   }

   //track total number of streams executed
   numStreamD++;

   //set previous stream ID and reset current stream to NULL
   prev_stream_id = loc->second;
   current_stream = NULL;
}

//This function is called before every block
VOID before_block(ADDRINT sa, UINT32 sl, void* insvalues,
                  UINT32 lscount, UINT32 img, UINT32 rtn)
{
   //increment counters
   numMemRef+=lscount;
   numIrefs+=sl;

   //create a new current_stream if needed
   if(current_stream == NULL) {
       current_stream = new stream_table_entry;
       current_stream->sa = sa;
       current_stream->sl = 0;
       current_stream->lscount = 0;
       current_stream->scount = 0;
       current_stream->nstream = 0;
       current_stream->img = img;
       current_stream->rtn = rtn;
   }

   //update current_stream values
   current_stream->sl += sl;
   current_stream->lscount += lscount;
   for(unsigned int i=0;i<sl;++i) {
      current_stream->insvalues.push_back(((int*)insvalues)[i]);
   }
}

//Pin calls this function every time a new basic block is encountered
VOID Trace(TRACE trace, VOID *v)
{
   RTN rtn = TRACE_Rtn(trace);
   UINT32 img_name_index=0, rtn_name_index=0;
   if(RTN_Valid(rtn)) {
      string rtn_name = RTN_Name(rtn);
      string img_name = IMG_Name(SEC_Img(RTN_Sec(rtn)));
      bool found = false;
      for(unsigned int i=0;i<img_name_list.size();++i) {
         if(img_name.compare(img_name_list[i])==0) {
            img_name_index = i;
            found = true;
            break;
         }
      }
      if(!found) {
         img_name_list.push_back(img_name);
         img_name_index = img_name_list.size()-1;
      }
      found = false;
      for(unsigned int i=0;i<rtn_name_list.size();++i) {
         if(rtn_name.compare(rtn_name_list[i])==0) {
            rtn_name_index = i;
            found = true;
            break;
         }
      }
      if(!found) {
	 rtn_name_list.push_back(rtn_name);
	 rtn_name_index = rtn_name_list.size()-1;
      }
   }

   //Visit every basic block in the trace
   for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
   {
       int* insvals = new int[BBL_NumIns(bbl)];
       int insctr = 0;
       //count memory referencing instructions
       UINT32 memory_refs = 0;
       Insval ins_value = INS_NORMAL;
       for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
           if (INS_IsMemoryRead(ins))
               ins_value = INS_READ;
           else if (INS_IsMemoryWrite(ins))
               ins_value = INS_WRITE;
           else
               ins_value = INS_NORMAL;
           insvals[insctr++] = ins_value;
           if(ins_value == INS_READ || ins_value == INS_WRITE)
               memory_refs++;

           if(INS_IsBranchOrCall(ins)) {
              INS_InsertCall(ins, IPOINT_TAKEN_BRANCH, (AFUNPTR)branch_taken,
                             IARG_ADDRINT, BBL_Address(bbl), IARG_END);
           }
       }

       //Insert a call to before_block before every bbl
       BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)before_block,
                      IARG_ADDRINT, BBL_Address(bbl),
                      IARG_UINT32,  BBL_NumIns(bbl),
                      IARG_PTR,     insvals,
                      IARG_UINT32,  memory_refs,
                      IARG_UINT32,  img_name_index,
                      IARG_UINT32,  rtn_name_index,
                      IARG_END);
   }
}

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
   "o", "streamcount.bin", "specify output file name");

//This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
   //FIXME: hacky way of making sure the last stream gets tidied up
   branch_taken(0);

   //Write to a file since cout and cerr maybe closed by the application
   ofstream OutFile;
   OutFile.open(KnobOutputFile.Value().c_str(),ofstream::binary);

   //write global stats
   int table_size = stream_table.size();
   OutFile.write(reinterpret_cast <const char*>(&(table_size)),sizeof(int));
   //write streams
   vector<stream_table_entry*>::iterator it;
   for(UINT32 i=0;i<stream_table.size();++i) {
       stream_table_entry* entry = stream_table[i];
       OutFile.write(reinterpret_cast <const char*>(&(entry->sa)),sizeof(ADDRINT));
       OutFile.write(reinterpret_cast <const char*>(&(entry->sl)),sizeof(UINT32));
       OutFile.write(reinterpret_cast <const char*>(&(entry->insvalues.at(0))),sizeof(int)*entry->sl);
       OutFile.write(reinterpret_cast <const char*>(&(entry->lscount)),sizeof(UINT32));
       OutFile.write(reinterpret_cast <const char*>(&(entry->scount)),sizeof(UINT32));
       const char* img_name = img_name_list[entry->img].c_str();
       UINT32 img_name_size = img_name_list[entry->img].size()+1;
       const char* rtn_name = rtn_name_list[entry->rtn].c_str();
       UINT32 rtn_name_size = rtn_name_list[entry->rtn].size()+1;
       OutFile.write(reinterpret_cast <const char*>(&(img_name_size)),sizeof(UINT32));
       OutFile.write(img_name,img_name_size);
       OutFile.write(reinterpret_cast <const char*>(&(rtn_name_size)),sizeof(UINT32));
       OutFile.write(rtn_name,rtn_name_size);
       int next_stream_size = entry->next_stream.size();
       OutFile.write(reinterpret_cast <const char*>(&(next_stream_size)),sizeof(int));
       for(map<UINT32,UINT32>::iterator it=entry->next_stream.begin();it!=entry->next_stream.end();++it) {
           OutFile.write(reinterpret_cast <const char*>(&(it->first)),sizeof(int));
           OutFile.write(reinterpret_cast <const char*>(&(it->second)),sizeof(int));
       }
   }
   OutFile.close();

   ofstream DebugFile;
   DebugFile.open("debug.txt");
   //write global stats
   DebugFile << "numStreamS: " << stream_table.size() << endl;
   DebugFile << "numStreamD: " << numStreamD << endl;
   DebugFile << "numMemRef: "  << numMemRef << endl;
   DebugFile << "numIrefs: " << numIrefs << endl;
   DebugFile << "maxStreamLen: " << maxStreamLen << endl;
   DebugFile << "avgStreamLen: " << (double)numIrefs/numStreamD << endl;

   //write streams
   for(UINT32 i=0;i<stream_table.size();++i) {
       stream_table_entry* entry = stream_table[i];

       DebugFile << i << ": (0x" << hex << entry->sa << ", " << dec <<
               entry->sl << ", " << entry->lscount << "), "
               << "(" << entry->scount << "); " ;

       DebugFile << "{ ";
       for(map<UINT32,UINT32>::iterator it=entry->next_stream.begin();it!=entry->next_stream.end();++it) {
           DebugFile << "(" << it->first << "," << it->second << ")";
       }
       DebugFile << " }" << endl;
   }
   DebugFile.close();

}

/* =====================================================================
*/
/* Print Help Message
*/
/* =====================================================================
*/

INT32 Usage()
{
   cerr << "This tool counts the number of streams executed" << endl;
   cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
   return -1;
}

/* =====================================================================
*/
/* Main
*/
/* =====================================================================
*/

int main(int argc, char * argv[])
{
   // Initialize symbol table code, needed for rtn instrumentation
   PIN_InitSymbols();

   //Initialize pin
   if (PIN_Init(argc, argv)) return Usage();

   //Register Instruction to be called to instrument instructions
   TRACE_AddInstrumentFunction(Trace, 0);

   //Register Fini to be called when the application exits
   PIN_AddFiniFunction(Fini, 0);

   //Start the program, never returns
   PIN_StartProgram();

   return 0;
}
