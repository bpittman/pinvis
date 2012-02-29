#include <osg/Node>
#include <osg/Group>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Texture2D>
#include <osgDB/ReadFile> 
#include <osgViewer/Viewer>
#include <osg/PositionAttitudeTransform>
#include <osgGA/TrackballManipulator>

#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <math.h>

using namespace std;

typedef uint32_t UINT32;
typedef UINT32 ADDRINT;

typedef struct {
   UINT32 sa; //stream starting address
   UINT32 sl; //stream length
   UINT32 scount; //stream count -- how many times it has been executed
   UINT32 lscount; //number of memory-referencing instructions
   UINT32 nstream; //number of unique next streams
   map<UINT32,UINT32> next_stream; //<stream index,times executed> count how many times the next stream is encountered
} stream_table_entry;

typedef pair<ADDRINT,UINT32> key; //<address of block,length of block>
typedef map<key,UINT32> stream_map; //<block key,index in stream_table>

static stream_map stream_ids; //maps block keys to their index in the stream_table
static vector<stream_table_entry*> stream_table; //one entry for each unique (by address & length) block

int main(int argc, char** argv)
{
   if(argc<2) {
      printf("Usage: pinvis <input file>\n");
      exit(1);
   }

   char* filename = argv[1];

   osgViewer::Viewer viewer;
   osg::Group* root = new osg::Group();
   osg::Geode* cubeGeode = new osg::Geode();
   osg::Geometry* cubeGeometry = new osg::Geometry();
   osg::Geode* crossGeode = new osg::Geode();
   osg::Geometry* crossGeometry = new osg::Geometry();

   //Associate the cube geometry with the cube geode 
   //   Add the cube geode to the root node of the scene graph.

   cubeGeode->addDrawable(cubeGeometry); 
   crossGeode->addDrawable(crossGeometry); 
   root->addChild(crossGeode);

   //Declare an array of vertices. Each vertex will be represented by 
   //a triple -- an instances of the vec3 class. An instance of 
   //osg::Vec3Array can be used to store these triples. Since 
   //osg::Vec3Array is derived from the STL vector class, we can use the
   //push_back method to add array elements. Push back adds elements to 
   //the end of the vector, thus the index of first element entered is 
   //zero, the second entries index is 1, etc.
   //Using a right-handed coordinate system with 'z' up, array 
   //elements zero..four below represent the 5 points required to create 
   //a simple cube.

   osg::Vec3Array* cubeVertices = new osg::Vec3Array;
                                                   // bottom
   cubeVertices->push_back( osg::Vec3(0, 0, 0)); // front left 
   cubeVertices->push_back( osg::Vec3(1, 0, 0)); // front right 
   cubeVertices->push_back( osg::Vec3(1, 1, 0)); // back right 
   cubeVertices->push_back( osg::Vec3(0, 1, 0)); // back left 

                                                   //top
   cubeVertices->push_back( osg::Vec3(0, 0, 1) ); // front left 
   cubeVertices->push_back( osg::Vec3(1, 0, 1) ); // front right 
   cubeVertices->push_back( osg::Vec3(1, 1, 1) ); // back right 
   cubeVertices->push_back( osg::Vec3(0, 1, 1) ); // back left

   float clen;
   clen = 12.0;
   osg::Vec3Array* crossVertices = new osg::Vec3Array;
   crossVertices->push_back (osg::Vec3(-clen, 0.0, 0.0));
   crossVertices->push_back (osg::Vec3( clen, 0.0, 0.0));
   crossVertices->push_back (osg::Vec3(  0.0, 0.0, -clen));
   crossVertices->push_back (osg::Vec3(  0.0, 0.0,  clen));  

   //Associate this set of vertices with the geometry associated with the 
   //geode we added to the scene.

   cubeGeometry->setVertexArray( cubeVertices );
   crossGeometry->setVertexArray (crossVertices);
   //Next, create a primitive set and add it to the cube geometry. 
   //Use the first four points of the cube to define the base using an 
   //instance of the DrawElementsUint class. Again this class is derived 
   //from the STL vector, so the push_back method will add elements in 
   //sequential order. To ensure proper backface cullling, vertices 
   //should be specified in counterclockwise order. The arguments for the 
   //constructor are the enumerated type for the primitive 
   //(same as the OpenGL primitive enumerated types), and the index in 
   //the vertex array to start from.

   osg::DrawElementsUInt* cubeBase = 
      new osg::DrawElementsUInt(osg::PrimitiveSet::QUADS, 0);
   cubeBase->push_back(3);
   cubeBase->push_back(2);
   cubeBase->push_back(1);
   cubeBase->push_back(0);
   cubeGeometry->addPrimitiveSet(cubeBase);

   osg::DrawElementsUInt* cross = 
      new osg::DrawElementsUInt(osg::PrimitiveSet::LINES, 0);
   cross->push_back(3);
   cross->push_back(2);
   cross->push_back(1);
   cross->push_back(0);
   crossGeometry->addPrimitiveSet(cross);

   //Repeat the same for each of the four sides. Again, vertices are 
   //specified in counter-clockwise order. 

   osg::DrawElementsUInt* cubeFaceOne = 
      new osg::DrawElementsUInt(osg::PrimitiveSet::QUADS, 0);
   cubeFaceOne->push_back(0);
   cubeFaceOne->push_back(1);
   cubeFaceOne->push_back(5);
   cubeFaceOne->push_back(4);
   cubeGeometry->addPrimitiveSet(cubeFaceOne);

   osg::DrawElementsUInt* cubeFaceTwo = 
      new osg::DrawElementsUInt(osg::PrimitiveSet::QUADS, 0);
   cubeFaceTwo->push_back(1);
   cubeFaceTwo->push_back(2);
   cubeFaceTwo->push_back(6);
   cubeFaceTwo->push_back(5);
   cubeGeometry->addPrimitiveSet(cubeFaceTwo);

   osg::DrawElementsUInt* cubeFaceThree = 
      new osg::DrawElementsUInt(osg::PrimitiveSet::QUADS, 0);
   cubeFaceThree->push_back(2);
   cubeFaceThree->push_back(3);
   cubeFaceThree->push_back(7);
   cubeFaceThree->push_back(6);
   cubeGeometry->addPrimitiveSet(cubeFaceThree);

   osg::DrawElementsUInt* cubeFaceFour = 
      new osg::DrawElementsUInt(osg::PrimitiveSet::QUADS, 0);
   cubeFaceFour->push_back(0);
   cubeFaceFour->push_back(3);
   cubeFaceFour->push_back(7);
   cubeFaceFour->push_back(4);
   cubeGeometry->addPrimitiveSet(cubeFaceFour);

   osg::DrawElementsUInt* cubeFaceFive =
      new osg::DrawElementsUInt(osg::PrimitiveSet::QUADS, 0);
   cubeFaceFive->push_back(4);
   cubeFaceFive->push_back(5);
   cubeFaceFive->push_back(6);
   cubeFaceFive->push_back(7);
   cubeGeometry->addPrimitiveSet(cubeFaceFive);
   //Declare and load an array of Vec4 elements to store colors. 

   osg::Vec4Array* colors = new osg::Vec4Array;
   colors->push_back(osg::Vec4(1.0f, 0.0f, 0.0f, 1.0f) ); //index 0 red
   colors->push_back(osg::Vec4(0.0f, 1.0f, 0.0f, 1.0f) ); //index 1 green
   colors->push_back(osg::Vec4(0.0f, 0.0f, 1.0f, 1.0f) ); //index 2 blue
   colors->push_back(osg::Vec4(1.0f, 1.0f, 1.0f, 1.0f) ); //index 3 white

   //Declare the variable that will match vertex array elements to color 
   //array elements. This vector should have the same number of elements 
   //as the number of vertices. This vector serves as a link between 
   //vertex arrays and color arrays. Entries in this index array 
   //coorespond to elements in the vertex array. Their values coorespond 
   //to the index in he color array. This same scheme would be followed 
   //if vertex array elements were matched with normal or texture 
   //coordinate arrays.
   //   Note that in this case, we are assigning 5 vertices to four 
   //   colors. Vertex array element zero (bottom left) and four (peak) 
   //   are both assigned to color array element zero (red).

   osg::TemplateIndexArray
      <unsigned int, osg::Array::UIntArrayType,4,4> *colorIndexArray;
   colorIndexArray = 
      new osg::TemplateIndexArray<unsigned int, osg::Array::UIntArrayType,4,4>;
   colorIndexArray->push_back(0); // vertex 0 assigned color array element 0
   colorIndexArray->push_back(1); // vertex 1 assigned color array element 1
   colorIndexArray->push_back(2); // vertex 2 assigned color array element 2
   colorIndexArray->push_back(3); // vertex 3 assigned color array element 3
   colorIndexArray->push_back(0); // vertex 4 assigned color array element 0
   colorIndexArray->push_back(1); // vertex 4 assigned color array element 1
   colorIndexArray->push_back(2); // vertex 4 assigned color array element 2
   colorIndexArray->push_back(3); // vertex 4 assigned color array element 3

   //The next step is to associate the array of colors with the geometry, 
   //assign the color indices created above to the geometry and set the 
   //binding mode to _PER_VERTEX.

   cubeGeometry->setColorArray(colors);
   cubeGeometry->setColorIndices(colorIndexArray);
   cubeGeometry->setColorBinding(osg::Geometry::BIND_PER_VERTEX);
   crossGeometry->setColorArray(colors);
   crossGeometry->setColorIndices(colorIndexArray);
   crossGeometry->setColorBinding(osg::Geometry::BIND_PER_VERTEX);

   //Now that we have created a geometry node and added it to the scene 
   //we can reuse this geometry. For example, if we wanted to put a 
   //second cube 15 units to the right of the first one, we could add 
   //this geode as the child of a transform node in our scene graph. 

   ifstream inFile;
   inFile.open(filename,ofstream::binary);

   UINT32 total_streams, dim;
   int row=0;
   int col=0;
   inFile.read((char*)&total_streams,sizeof(UINT32));
   dim = ceil(sqrt(total_streams));

   for(int i=0;i<total_streams;++i) {
      stream_table_entry* e = new stream_table_entry;
      inFile.read((char*)(&(e->sa)),sizeof(ADDRINT));
      inFile.read((char*)(&(e->sl)),sizeof(UINT32));
      inFile.read((char*)(&(e->lscount)),sizeof(UINT32));
      inFile.read((char*)(&(e->scount)),sizeof(UINT32));
      int next_stream_count;
      inFile.read((char*)&next_stream_count,sizeof(int));
      for(int j=0;j<next_stream_count;++j) {
         int stream_index, times_executed;
         inFile.read((char*)&stream_index,sizeof(UINT32));
         inFile.read((char*)&times_executed,sizeof(UINT32));
         e->next_stream.insert(pair<UINT32,UINT32>(stream_index,times_executed));
      }
      // Declare and initialize a transform node.
      osg::PositionAttitudeTransform* cubeXForm =
	 new osg::PositionAttitudeTransform();

      // Use the 'addChild' method of the osg::Group class to
      // add the transform as a child of the root node and the
      // cube node as a child of the transform.

      root->addChild(cubeXForm);
      cubeXForm->addChild(cubeGeode);

      // Declare and initialize a Vec3 instance to change the
      // position of the model in the scene
      osg::Vec3 cubePosition(row,col,0);
      osg::Vec3 cubeScale(1,1,e->sl);
      cubeXForm->setPosition(cubePosition);
      cubeXForm->setScale(cubeScale);
      if(++row>=dim) {
         row=0;
         col++;
      }
   }

   //The final step is to set up and enter a simulation loop.

   viewer.setSceneData( root );
   //viewer.run();
        
   viewer.setCameraManipulator(new osgGA::TrackballManipulator());
   viewer.realize();

   while( !viewer.done() )
   {
      viewer.frame();
   } 

   return 0;
}
