#include <osg/Node>
#include <osg/Group>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Texture2D>
#include <osgDB/ReadFile> 
#include <osgViewer/Viewer>
#include <osg/PositionAttitudeTransform>
#include <osgGA/TrackballManipulator>
#include <osg/ShapeDrawable>
#include <osgText/Text>
#include <osg/io_utils>

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
   char* img_name;
   char* rtn_name;
   map<UINT32,UINT32> next_stream; //<stream index,times executed> count how many times the next stream is encountered
} stream_table_entry;

typedef pair<ADDRINT,UINT32> key; //<address of block,length of block>
typedef map<key,UINT32> stream_map; //<block key,index in stream_table>

static stream_map stream_ids; //maps block keys to their index in the stream_table
static vector<stream_table_entry*> stream_table; //one entry for each unique (by address & length) block

// class to handle events with a pick
class PickHandler : public osgGA::GUIEventHandler {
public:

    PickHandler(osgText::Text* updateText):
        _updateText(updateText) {}

    ~PickHandler() {}

    bool handle(const osgGA::GUIEventAdapter& ea,osgGA::GUIActionAdapter& aa);

    virtual void pick(osgViewer::View* view, const osgGA::GUIEventAdapter& ea);

    void setLabel(const std::string& name)
    {
        if (_updateText.get()) _updateText->setText(name);
    }

protected:

    osg::ref_ptr<osgText::Text>  _updateText;
};

bool PickHandler::handle(const osgGA::GUIEventAdapter& ea,osgGA::GUIActionAdapter& aa)
{
    switch(ea.getEventType())
    {
        case(osgGA::GUIEventAdapter::PUSH):
        {
            osgViewer::View* view = dynamic_cast<osgViewer::View*>(&aa);
            if (view) pick(view,ea);
            return false;
        }
        case(osgGA::GUIEventAdapter::KEYDOWN):
        {
            if (ea.getKey()=='c')
            {
                osgViewer::View* view = dynamic_cast<osgViewer::View*>(&aa);
                osg::ref_ptr<osgGA::GUIEventAdapter> event = new osgGA::GUIEventAdapter(ea);
                event->setX((ea.getXmin()+ea.getXmax())*0.5);
                event->setY((ea.getYmin()+ea.getYmax())*0.5);
                if (view) pick(view,*event);
            }
            return false;
        }
        default:
            return false;
    }
}

void PickHandler::pick(osgViewer::View* view, const osgGA::GUIEventAdapter& ea)
{
    osgUtil::LineSegmentIntersector::Intersections intersections;

    std::string gdlist="";
    float x = ea.getX();
    float y = ea.getY();
#if 0
    osg::ref_ptr< osgUtil::LineSegmentIntersector > picker = new osgUtil::LineSegmentIntersector(osgUtil::Intersector::WINDOW, x, y);
    osgUtil::IntersectionVisitor iv(picker.get());
    view->getCamera()->accept(iv);
    if (picker->containsIntersections())
    {
        intersections = picker->getIntersections();
#else
    if (view->computeIntersections(x,y,intersections))
    {
#endif
        for(osgUtil::LineSegmentIntersector::Intersections::iterator hitr = intersections.begin();
            hitr != intersections.end();
            ++hitr)
        {
            std::ostringstream os;
            if (!hitr->nodePath.empty() && !(hitr->nodePath.back()->getName().empty()))
            {
                // the geodes are identified by name.
                os<<"Object \""<<hitr->nodePath.back()->getName()<<"\""<<std::endl;
            }
            else if (hitr->drawable.valid())
            {
                os<<"Object \""<<hitr->drawable->className()<<"\""<<std::endl;
            }

            os<<"        local coords vertex("<< hitr->getLocalIntersectPoint()<<")"<<"  normal("<<hitr->getLocalIntersectNormal()<<")"<<std::endl;
            os<<"        world coords vertex("<< hitr->getWorldIntersectPoint()<<")"<<"  normal("<<hitr->getWorldIntersectNormal()<<")"<<std::endl;
            const osgUtil::LineSegmentIntersector::Intersection::IndexList& vil = hitr->indexList;
            for(unsigned int i=0;i<vil.size();++i)
            {
                os<<"        vertex indices ["<<i<<"] = "<<vil[i]<<std::endl;
            }

            gdlist += os.str();
        }
    }
    setLabel(gdlist);
}

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
   osg::Geode* crossGeode = new osg::Geode();
   osg::Geometry* crossGeometry = new osg::Geometry();

   //Associate the cube geometry with the cube geode 
   //   Add the cube geode to the root node of the scene graph.

   cubeGeode->addDrawable(new osg::ShapeDrawable(new osg::Box(osg::Vec3(0.5,0.5,0.5),1.0,1.0,1.0))); 
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

   float clen;
   clen = 12.0;
   osg::Vec3Array* crossVertices = new osg::Vec3Array;
   crossVertices->push_back (osg::Vec3(-clen, 0.0, 0.0));
   crossVertices->push_back (osg::Vec3( clen, 0.0, 0.0));
   crossVertices->push_back (osg::Vec3(  0.0, 0.0, -clen));
   crossVertices->push_back (osg::Vec3(  0.0, 0.0,  clen));  

   //Associate this set of vertices with the geometry associated with the 
   //geode we added to the scene.

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

   osg::DrawElementsUInt* cross = 
      new osg::DrawElementsUInt(osg::PrimitiveSet::LINES, 0);
   cross->push_back(3);
   cross->push_back(2);
   cross->push_back(1);
   cross->push_back(0);
   crossGeometry->addPrimitiveSet(cross);

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
   int img_size=0,rtn_size=0;
   inFile.read((char*)&total_streams,sizeof(UINT32));
   dim = ceil(sqrt(total_streams));

   for(int i=0;i<total_streams;++i) {
      stream_table_entry* e = new stream_table_entry;
      inFile.read((char*)(&(e->sa)),sizeof(ADDRINT));
      inFile.read((char*)(&(e->sl)),sizeof(UINT32));
      inFile.read((char*)(&(e->lscount)),sizeof(UINT32));
      inFile.read((char*)(&(e->scount)),sizeof(UINT32));
      inFile.read((char*)(&(img_size)),sizeof(UINT32));
      e->img_name = new char[img_size];
      inFile.read(e->img_name,img_size);
      inFile.read((char*)(&(rtn_size)),sizeof(UINT32));
      e->rtn_name = new char[rtn_size];
      inFile.read(e->rtn_name,rtn_size);
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
