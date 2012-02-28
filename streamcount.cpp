#include <iostream>
#include <fstream>
#include <map>
#include <vector>

#include "pin.H"

using namespace std;

typedef struct {
   ADDRINT sa; //stream starting address
   UINT32  sl; //stream length
   UINT32  scount; //stream count -- how many times it has been executed
   UINT32  lscount; //number of memory-referencing instructions
   UINT32  nstream; //number of unique next streams
   map<UINT32,UINT32> next_stream; //<stream index,times executed> count how many times the next stream is encountered
} stream_table_entry;

typedef pair<ADDRINT,UINT32> key; //<address of block,length of block>
typedef map<key,UINT32> stream_map; //<block key,index in stream_table>

static stream_map stream_ids; //maps block keys to their index in the stream_table
static vector<stream_table_entry*> stream_table; //one entry for each unique (by address & length) block

static UINT32 numStreamD = 0; //number of program streams executed (dynamic)
static UINT32 numMemRef = 0; //number of memory referencing instructions executed (dynamic)
static UINT32 numIrefs = 0; //number of instructions executed (dynamic)
static UINT32 maxStreamLen = 0; //max stream length (max # of instructions executed in sequence w/o branch)

static INT32 prev_stream_id = -1; //the previously executed stream's index in the stream_table

//This function is called before every block
VOID before_block(ADDRINT sa, UINT32 sl, UINT32 lscount)
{
   //increment counters
   numStreamD++;
   numMemRef+=lscount;
   numIrefs+=sl;
   if(sl>maxStreamLen) maxStreamLen = sl;

   //find entry in stream_table or create a new entry
   key k(sa,sl);
   stream_map::iterator loc = stream_ids.find(k);
   if(loc == stream_ids.end()) {
       stream_table_entry* e = new stream_table_entry;
       e->scount = 0;
       e->sa = sa;
       e->sl = sl;
       e->lscount = lscount;
       stream_table.push_back(e);
       stream_ids.insert(pair<key,UINT32>(k, stream_table.size()-1));
       loc = stream_ids.find(k);
   }

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

   //increment stream counter, and set the prev_stream_id before moving on
   prev_stream_id = loc->second;
   stream_table[loc->second]->scount++;
}

//Pin calls this function every time a new basic block is encountered
VOID Trace(TRACE trace, VOID *v)
{
   //Visit every basic block in the trace
   for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
   {
       //count memory referencing instructions
       UINT32 memory_refs = 0;
       for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
           if (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins))
               memory_refs++;
       }

       //Insert a call to before_block before every bbl
       BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)before_block,
                      IARG_ADDRINT, BBL_Address(bbl),
                      IARG_UINT32,  BBL_NumIns(bbl),
                      IARG_UINT32,  memory_refs,
                      IARG_END);
   }
}

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
   "o", "streamcount.out", "specify output file name");

//This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
   //Write to a file since cout and cerr maybe closed by the application
   ofstream OutFile;
   OutFile.open(KnobOutputFile.Value().c_str());
   OutFile.setf(ios::showbase);

   //write global stats
   OutFile << "numStreamS: " << stream_table.size() << endl;
   OutFile << "numStreamD: " << numStreamD << endl;
   OutFile << "numMemRef: "  << numMemRef << endl;
   OutFile << "numIrefs: " << numIrefs << endl;
   OutFile << "maxStreamLen: " << maxStreamLen << endl;
   OutFile << "avgStreamLen: " << (double)numIrefs/numStreamD << endl;

   //write streams
   vector<stream_table_entry*>::iterator it;
   for(UINT32 i=0;i<stream_table.size();++i) {
       stream_table_entry* entry = stream_table[i];

       OutFile << i << ": (0x" << hex << entry->sa << ", " << dec <<
               entry->sl << ", " << entry->lscount << "), "
               << "(" << entry->scount << "); " ;

       OutFile << "{ ";
       for(map<UINT32,UINT32>::iterator it=entry->next_stream.begin();it!=entry->next_stream.end();++it) {
           OutFile << "(" << it->first << "," << it->second << ")";
       }
       OutFile << " }" << endl;
   }
   OutFile.close();
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
