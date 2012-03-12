#include <osg/Node>
#include <osg/Group>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Material>
#include <osg/Texture2D>
#include <osg/AnimationPath>
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
   int* insvalues; //array of Insvals, one for each instruction
   UINT32 scount; //stream count -- how many times it has been executed
   UINT32 lscount; //number of memory-referencing instructions
   UINT32 nstream; //number of unique next streams
   char* img_name;
   char* rtn_name;
   map<UINT32,UINT32> next_stream; //<stream index,times executed> count how many times the next stream is encountered
   osg::PositionAttitudeTransform** transforms; //array of transforms, one transform per instruction
   osg::AnimationPath** animationPaths; //array of animation paths, one path per instruction
} stream_table_entry;

typedef pair<ADDRINT,UINT32> key; //<address of block,length of block>
typedef map<key,UINT32> stream_map; //<block key,index in stream_table>

enum Insval { INS_NORMAL, INS_READ, INS_WRITE };
enum PlacementScheme { GRID_LAYOUT, ROW_LAYOUT };
enum ColorScheme { MEMORY_COLORING };

static stream_map stream_ids; //maps block keys to their index in the stream_table
static vector<stream_table_entry*> stream_table; //one entry for each unique (by address & length) block
static vector<osg::Node*> highlighted; //nodes that are currently highlighted by the picking code

void clearHighlighted(void);
void setColor(osg::Node*,float r, float g, float b);
void placeStreams(int scheme);
void colorStreams(int scheme);

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
    if (view->computeIntersections(x,y,intersections))
    {
        for(osgUtil::LineSegmentIntersector::Intersections::iterator hitr = intersections.begin();
            hitr != intersections.end();
            ++hitr)
        {
            std::ostringstream os;
            if(hitr->nodePath.size() >= 2) {
                osg::Node *node = hitr->nodePath[hitr->nodePath.size()-2];
                osg::StateSet *ss = node->getOrCreateStateSet();
                osg::Material *nm = new osg::Material;
                nm->setDiffuse(osg::Material::FRONT,osg::Vec4(0.9f,.2f,.2f,1.0f));
                ss->setAttribute(nm);
                os << "Function \"" << node->getName()<<"\""<<endl;
                clearHighlighted();
                highlighted.push_back(node);
            }
            else if (!hitr->nodePath.empty() && !(hitr->nodePath.back()->getName().empty()))
            {
                // the geodes are identified by name.
                os<<"Object \""<<hitr->nodePath.back()->getName()<<"\""<<std::endl;
            }
            else if (hitr->drawable.valid())
            {
                os<<"Object \""<<hitr->drawable->className()<<"\""<<std::endl;
            }

            gdlist += os.str();
            break; //for now just grab the first intersection; seems to be the closest to camera anyway
        }
    }
    setLabel(gdlist);
}

class KeyboardEventHandler : public osgGA::GUIEventHandler
{
    public:
       virtual bool handle(const osgGA::GUIEventAdapter& ea,osgGA::GUIActionAdapter&);
       virtual void accept(osgGA::GUIEventHandlerVisitor& v)   { v.visit(*this); };
};

bool KeyboardEventHandler::handle(const osgGA::GUIEventAdapter& ea,osgGA::GUIActionAdapter& aa)
{
   switch(ea.getEventType())
   {
      case(osgGA::GUIEventAdapter::KEYDOWN):
      {
          switch(ea.getKey())
          {
             case '1':
                placeStreams(GRID_LAYOUT);
                return false;
                break;
             case '2':
                placeStreams(ROW_LAYOUT);
                return false;
                break;
             default:
                return false;
          }
      }
      default:
         return false;
   }
}


void clearHighlighted(void) {
   for(int i=0;i<highlighted.size();++i) {
      setColor(highlighted[i],1.0,1.0,1.0);
   }
}

void setColor(osg::Node* node,float r, float g, float b) {
   osg::StateSet *ss = node->getOrCreateStateSet();
   osg::Material *nm = new osg::Material;
   nm->setDiffuse(osg::Material::FRONT,osg::Vec4(r,g,b,1.0f));
   ss->setAttribute(nm);
}

osg::Node* createHUD(osgText::Text* updateText)
{

    // create the hud. derived from osgHud.cpp
    // adds a set of quads, each in a separate Geode - which can be picked individually
    // eg to be used as a menuing/help system!
    // Can pick texts too!

    osg::Camera* hudCamera = new osg::Camera;
    hudCamera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    hudCamera->setProjectionMatrixAsOrtho2D(0,1600,0,900);
    hudCamera->setViewMatrix(osg::Matrix::identity());
    hudCamera->setRenderOrder(osg::Camera::POST_RENDER);
    hudCamera->setClearMask(GL_DEPTH_BUFFER_BIT);

    // turn lighting off for the text and disable depth test to ensure its always ontop.
    osg::Vec3 position(150.0f,800.0f,0.0f);
    osg::Vec3 delta(-60.0f,-60.0f,0.0f);

    {
        osg::Geode* geode = new osg::Geode();
        osg::StateSet* stateset = geode->getOrCreateStateSet();
        stateset->setMode(GL_LIGHTING,osg::StateAttribute::OFF);
        stateset->setMode(GL_DEPTH_TEST,osg::StateAttribute::OFF);
        geode->setName("simple");
        hudCamera->addChild(geode);

        osgText::Text* text = new  osgText::Text;
        geode->addDrawable( text );

        text->setText("PINVIS");
        text->setPosition(position);

        position += delta;
    }

    { // this displays what has been selected
        osg::Geode* geode = new osg::Geode();
        osg::StateSet* stateset = geode->getOrCreateStateSet();
        stateset->setMode(GL_LIGHTING,osg::StateAttribute::OFF);
        stateset->setMode(GL_DEPTH_TEST,osg::StateAttribute::OFF);
        geode->setName("The text label");
        geode->addDrawable( updateText );
        hudCamera->addChild(geode);

        updateText->setCharacterSize(20.0f);
        updateText->setColor(osg::Vec4(1.0f,1.0f,0.0f,1.0f));
        updateText->setText("");
        updateText->setPosition(position);
        updateText->setDataVariance(osg::Object::DYNAMIC);

        position += delta;
    }

    return hudCamera;
}

void placeStreams(int scheme) {
   //a grid with a column in each cell representing each stream
   if(scheme == GRID_LAYOUT) {
      int dim = ceil(sqrt(stream_table.size()));
      int row=0;
      int col=0;
      for(int i=0;i<stream_table.size();++i) {
         for(int j=0;j<stream_table[i]->sl;++j) {
            osg::AnimationPath* ap = stream_table[i]->animationPaths[j];
            ap->setLoopMode( osg::AnimationPath::NO_LOOPING );
            osg::Vec3 curPos = stream_table[i]->transforms[j]->getPosition();
            ap->insert(0.0f,osg::AnimationPath::ControlPoint(curPos));
            ap->insert(1.0f,osg::AnimationPath::ControlPoint(osg::Vec3(row,col,j)));
            stream_table[i]->transforms[j]->setUpdateCallback(new osg::AnimationPathCallback(ap));
         }
	 if(++row>=dim) {
	    row=0;
	    col++;
	 }
      }
   }

   //2d row layout
   else if(scheme == ROW_LAYOUT) {
      int row=0;
      int col=0;
      for(int i=0;i<stream_table.size();++i) {
	 for(int j=0;j<stream_table[i]->sl;++j) {
            osg::AnimationPath* ap = stream_table[i]->animationPaths[j];
            ap->setLoopMode( osg::AnimationPath::NO_LOOPING );
            osg::Vec3 curPos = stream_table[i]->transforms[j]->getPosition();
            ap->insert(0.0f,osg::AnimationPath::ControlPoint(osg::Vec3(curPos)));
            ap->insert(1.0f,osg::AnimationPath::ControlPoint(osg::Vec3(row,0,col++)));
            stream_table[i]->transforms[j]->setUpdateCallback(new osg::AnimationPathCallback(ap));
	 }
         row++;
         col=0;
      }
   }
}

void colorStreams(int scheme) {
   //green==memory read, red==memory write, white==no memory access
   if(scheme == MEMORY_COLORING) {
      for(int i=0;i<stream_table.size();++i) {
         for(int j=0;j<stream_table[i]->sl;++j) {
	    if(stream_table[i]->insvalues[j] == INS_NORMAL)
	       setColor(stream_table[i]->transforms[j],1.0,1.0,1.0);
	    else if(stream_table[i]->insvalues[j] == INS_READ)
	       setColor(stream_table[i]->transforms[j],0.0,1.0,0.0);
	    else if(stream_table[i]->insvalues[j] == INS_WRITE)
	       setColor(stream_table[i]->transforms[j],1.0,0.0,0.0);
         }
      }
   }
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

   //Associate the cube geometry with the cube geode 
   //   Add the cube geode to the root node of the scene graph.

   cubeGeode->addDrawable(new osg::ShapeDrawable(new osg::Box(osg::Vec3(0.5,0.5,0.5),1.0,1.0,1.0))); 

   osg::ref_ptr<osgText::Text> updateText = new osgText::Text;
   root->addChild(createHUD(updateText.get()));
   viewer.addEventHandler(new PickHandler(updateText.get()));
   viewer.addEventHandler(new KeyboardEventHandler());

   ifstream inFile;
   inFile.open(filename,ofstream::binary);

   UINT32 total_streams, dim;
   int img_size=0,rtn_size=0;
   inFile.read((char*)&total_streams,sizeof(UINT32));

   for(int i=0;i<total_streams;++i) {
      stream_table_entry* e = new stream_table_entry;
      inFile.read((char*)(&(e->sa)),sizeof(ADDRINT));
      inFile.read((char*)(&(e->sl)),sizeof(UINT32));
      e->insvalues = new int[e->sl];
      inFile.read((char*)(e->insvalues),sizeof(int)*e->sl);
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
      e->transforms = new osg::PositionAttitudeTransform*[e->sl];
      e->animationPaths = new osg::AnimationPath*[e->sl];

      for(int j=0;j<e->sl;++j) {
         // Declare and initialize transform nodes.
         e->transforms[j] = new osg::PositionAttitudeTransform();
         e->transforms[j]->setPosition(osg::Vec3(0,0,0));
         e->animationPaths[j] = new osg::AnimationPath();

         // Use the 'addChild' method of the osg::Group class to
         // add the transform as a child of the root node and the
         // cube node as a child of the transform.

         root->addChild(e->transforms[j]);
         e->transforms[j]->addChild(cubeGeode);
         ostringstream name;
         name << e->img_name << ":" << e->rtn_name << " " << e->sl;
         e->transforms[j]->setName(name.str());
      }
      stream_table.push_back(e);
   }

   placeStreams(GRID_LAYOUT);
   colorStreams(MEMORY_COLORING);

   //The final step is to set up and enter a simulation loop.

   viewer.setSceneData( root );
   //viewer.run();
        
   viewer.setCameraManipulator(new osgGA::TrackballManipulator());
   viewer.setUpViewInWindow(0,0,1024,768);
   viewer.realize();

   while( !viewer.done() )
   {
      viewer.frame();
   } 

   return 0;
}
