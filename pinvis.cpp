#include <osg/Node>
#include <osg/Group>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Material>
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
   osg::PositionAttitudeTransform* transform;
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
    osg::Vec3 delta(0.0f,-60.0f,0.0f);

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
      e->transform = new osg::PositionAttitudeTransform();

      // Use the 'addChild' method of the osg::Group class to
      // add the transform as a child of the root node and the
      // cube node as a child of the transform.

      root->addChild(e->transform);
      e->transform->addChild(cubeGeode);
      ostringstream name;
      name << e->img_name << ":" << e->rtn_name << " " << e->sl;
      e->transform->setName(name.str());

      // Declare and initialize a Vec3 instance to change the
      // position of the model in the scene
      osg::Vec3 cubePosition(row,col,0);
      osg::Vec3 cubeScale(1,1,e->sl);
      e->transform->setPosition(cubePosition);
      e->transform->setScale(cubeScale);
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
