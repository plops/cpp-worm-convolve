//============================================================================
// Name        : wormcores.cpp
// Author      : Martin Kielhorn
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <cvd/image_io.h>
#include <cvd/videodisplay.h>
#include <cvd/glwindow.h>
#include <cvd/gl_helpers.h>
#include <cvd/draw.h>
#include <cvd/morphology.h>
#include <cvd/connected_components.h>
#include <TooN/TooN.h>

using namespace CVD;
using namespace TooN;
using namespace std;
/* confocal test data: c/ *.tif

And the voxel size is 0.198 µm in X and Y and 1µm in Z

taken stuff from
git://github.com/paulreimer/ofxMarsyasApps.git
(in google codesearch)

and libcvd/test/qfloodfill_test.cc
for color scaling:
0215/tif.lisp
 */

Image<byte> threshold(Image<byte>in,int thresh)
{
  Image<byte>result(in.size());
  for(int y=0; y < in.size().y; y++)
    for(int x=0; x < in.size().x; x++)
      if(in[y][x]<thresh)
	result[y][x] = 0;
      else
	result[y][x] = 255;
  return result;
}

Image<byte> find_components(Image<byte> in,vector<vector<ImageRef> >&v)
{

  vector<ImageRef> p;
  for(int y=0; y < in.size().y; y++)
    for(int x=0; x < in.size().x; x++)
      if(in[y][x])
	p.push_back(ImageRef(x,y));
  connected_components(p,
		       v);

  Image<byte> result(in.size());
  int count=0;
  for(unsigned int j=0; j <v.size(); j++)
    if(v[j].size()>2){
      count++;
      for(unsigned int k=0; k <v[j].size(); k++)
	result[v[j][k]]=65+count;
    }
  return result;
}


Image<byte> find_components(Image<byte>in) // overload for optional parameter
{
  vector<vector<ImageRef> >v;
  return find_components(in,v);
}


void
load_ids(char*fn,vector<Image<unsigned short> > &in)
{
  Image<unsigned short> slice(ImageRef(512,512));
  FILE*f=fopen(fn,"r");
  for(int i=0;i<29;i++){
    int n=fread(slice.data(),512,512*2,f);
    if(n!=2*512)
      cerr<<"error: file ended early at slice "<<i<<" with "<<n<<endl;
    in.push_back(slice.copy_from_me());
  }
  fclose(f);
}

/* load image to graphics memory and return handle */
unsigned int
upload_image(Image<byte>&temp)
{
  unsigned int tex;
  glGenTextures(1,&tex);
  const int target=GL_TEXTURE_RECTANGLE_NV;
  glBindTexture(target,tex);

  //glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  glTexParameterf( target, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
  glTexParameterf( target, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
  glPixelStorei(GL_UNPACK_ALIGNMENT,1);
  glTexImage2D(temp, 0, target);
  return tex;
}

/* bind the texture and draw quad with size x\times y */
unsigned int
draw_image(uint tex,int x, int y)
{
  const int target=GL_TEXTURE_RECTANGLE_NV;
  glBindTexture(target,tex);
  glEnable(target);
  glBegin(GL_QUADS);
  glTexCoord2i(0, 0);
  glVertex2i(0,0);
  glTexCoord2i(x, 0);
  glVertex2i(x,0);
  glTexCoord2i(x,y);
  glVertex2i(x,y);
  glTexCoord2i(0,y);
  glVertex2i(0,y);
  glEnd ();
  glDisable(target);
  return tex;
}

/* load image to graphics memory, draw quad and delete */
void
draw_image_once(Image<byte> &temp)
{
  unsigned int tex=upload_image(temp);
  draw_image(tex,temp.size().x,temp.size().y);
  glDeleteTextures(1,&tex);
}

/* read a tiff stack into a vector of images */
void
load_tif(vector<Image<byte> >&in)
{
  /* read images xaaa.tif .. xabh.tif */
  for(int i=0;i<34;i++){
    char s[100];
    sprintf(s,"/home/martin/0510/c/xa%c%c.tif",'a'+i/26,'a'+i%26);
    in.push_back(img_load(s));
  }
  cout << "read image with size " << in[0].size().x <<
    "x" << in[0].size().y << "x" <<in.size()<<endl;
}

inline int min(int a,int b)
{
  return a<b?a:b;
}

/* store stack as a 3d texture in graphics memory */
unsigned int
upload_stack(vector<Image<byte> >&in)
{
  const int target=GL_TEXTURE_3D;
  enum {TW=256,TH=256,TZ=32};
  int
    W=in[0].size().x,
    H=in[0].size().y,
    Z=in.size(),
    w=min(TW,W),
    h=min(TH,H),
    z=min(TZ,Z);
  byte*buf=new byte [TW*TH*TZ];
  int i,j,k;
  for(k=0;k<z;k++)
    for(j=0;j<h;j++)
      for(i=0;i<w;i++)
	buf[i+TW*(j+TH*k)]=in[k][j][i];
  unsigned int tex;
  glGenTextures(1,&tex);
  glBindTexture(target,tex);
  glTexParameteri( target, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri( target, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexParameteri( target, GL_TEXTURE_WRAP_R, GL_CLAMP);
  glTexParameterf( target, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
  glTexParameterf( target, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
  glTexImage3D(target,0,GL_LUMINANCE,TW,TH,TZ,
	       0,GL_LUMINANCE,GL_UNSIGNED_BYTE,buf);
  delete[] buf;
  return tex;
}

/* draw a cube filled with quads for volume rendering */
void
draw_stack(unsigned int tex)
{
  const int target=GL_TEXTURE_3D;
  glBindTexture(target,tex);
  glEnable(target);
  glBegin(GL_QUADS);
  float x=5, y=x;
  const int n=100;
  for(int z=0;z<n;z++){
    float zz=z*1./n; //z*x/0.198;
    float zx=zz*x*.198;
    glTexCoord3f(-.4,-.4,zz);    glVertex3f(0,0,zx);
    glTexCoord3f(1.4,-.4,zz);    glVertex3f(x,0,zx);
    glTexCoord3f(1.4,1.4,zz);    glVertex3f(x,y,zx);
    glTexCoord3f(-.4,1.4,zz);    glVertex3f(0,y,zx);
  }
  glEnd ();
  glDisable(target);
}

/* traverse cuboidal region of a volume
   x,y,z \el [0..(w-1)]
   X,Y,Z \el [0..(w-1)]
 */
void
doimg6i(vector<Image<byte> >&in,int X,int Y,int Z,int x,int y,int z)
{
  int i,j,k;
  cout << "doimg "
       << x << "x"
       << y << "x"
       << z << "x ->"
       << X << "x"
       << Y << "x"
       << Z << "x" << endl;

  for(k=z;k<Z;k++)
    for(j=y;j<Y;j++)
      for(i=x;i<X;i++)
	in[k][j][i]=255;
}

inline float
clamp(float a,float m,float M)
{
  if(a<m)
    return m;
  if(a>M)
    return M;
  return a;
}

/* do something with a cuboidal region of a volume
   X=1. is maximum
   x=0. is minimum */
void
doimg(vector<Image<byte> >&in,float X,float Y,float Z,
		float x=0.,float y=0.,float z=0.)
{
  int w=in[0].size().x,h=in[0].size().y,d=in.size();
  doimg6i(in,
	  floor(clamp(X*w,1,w-1)),
	  floor(clamp(Y*h,1,h-1)),
	  floor(clamp(Z*d,1,d-1)),
	  floor(clamp(x*w,0,w-1)),
	  floor(clamp(y*h,0,h-1)),
	  floor(clamp(z*d,0,d-1)));
}

void
switch_3d(int w,int h)
{
	  glLoadIdentity();
	  glViewport(0,0,w,h);
	  glMatrixMode(GL_PROJECTION);
	  glLoadIdentity();
	  gluPerspective(30,w*1./h,.1,1000);
	  gluLookAt(10,10,10, .5,.5,1, 0,0,1);
	  glMatrixMode(GL_MODELVIEW);
	  glLoadIdentity();
}

void
switch_2d(int w,int h)
{
	  glLoadIdentity();
	  glViewport(0,0,w,h);
	  glMatrixMode(GL_PROJECTION);
	  glLoadIdentity();
	  glOrtho(0,w,0,h,-100,100);
	  gluPerspective(40,w*1./h,.1,1000);
	  gluLookAt(10,10,10, 0,0,0, 0,0,1);
	  glMatrixMode(GL_MODELVIEW);
	  glLoadIdentity();
}
void
switch_cvd(int w,int h)
{
	glLoadIdentity();
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glColor3f(1.0f,1.0f,1.0f);
	glRasterPos2f(-1, 1);
	glOrtho(-0.375, w-0.375, h-.375, -0.375, -1 , 1); //offsets to make (0,0) the top left pixel (rather than off the display)
	glPixelZoom(1,-1);
}

/* create a vector from spherical angles given in degree
 */
Vector<3>
direction(double theta,double phi)
{
  double t=theta*M_PI/180,p=phi*M_PI/180,st=sin(t);
  return makeVector(st*cos(p),st*sin(p),cos(t));
}

/* minimal distance of a point to a line
   http://local.wasp.uwa.edu.au/~pbourke/geometry/pointline/
*/
double
mindist(Vector<3> point, Vector<3> origin, Vector<3> dir)
{
  double u=((point-origin)*dir)/(dir*dir);
  Vector<3> po=origin+u*dir-point;
  return sqrt(po*po);
}

/* draw a cylinder with radius r and angles theta,
 * phi (in degree) through the volume.
 */
void
draw_cylinder(vector<Image<byte> > &in, Vector<3> center, double r, double theta, double phi)
{
  int
    w=in[0].size().x,
    h=in[0].size().y,
    z=in.size(),
    i,j,k;
  Vector<3>
    dir=direction(theta,phi),
    origin=.5*makeVector(w,h,z)+center;
  double s=1/0.198;
  for(k=0;k<z;k++)
    for(j=0;j<h;j++)
      for(i=0;i<w;i++){
	Vector<3> point=makeVector(i,j,k*s);
	if(fabs(mindist(point,origin,dir))<r)
	  in[k][j][i]=255;
      }
}

int main()
{
  vector<Image<byte> > in;
  load_tif(in);

  vector<Image<byte> > comps,thr,medians;
  vector<ImageRef> element=getDisc(6);

  for(int i=0;i<in.size();i++){
    thr.push_back(threshold(in[i],14));
    Image<byte> med(thr[0].size());
    morphology(thr[thr.size()-1], element, Morphology::Median<byte>(), med);
    medians.push_back(med);
    comps.push_back(find_components(med));
  }
  int w=1024,h=700;
  GLWindow window(ImageRef(w,h));
  glClearColor(.1,.1,.1,1);
  //  doimg(medians,.1,.1,1.);
  //  doimg(medians,0.1,1.,0.1);
  //  doimg(medians,1.,0.1,0.1);

  cout <<"finished"<<endl;
  draw_cylinder(medians,makeVector(10,10,0),10,76,7);
  uint stack=upload_stack(medians);
  for(int c=0;c<100;c++){
    for(int j=0;j<29;j++){
      glClear(GL_COLOR_BUFFER_BIT);
      switch_cvd(w,h);

      glRasterPos2f(0,0);

      //glMatrixMode(GL_COLOR);
      //glPushMatrix();{
      //glScalef(30,30,30);
      glDrawPixels(in[j]);
      //}glPopMatrix();

      glRasterPos2f(512,0);
      glDrawPixels(comps[j]);

	  glPushMatrix();
	  {
		  //superimpose some images
		  glEnable(GL_BLEND);
		  glBlendFunc(GL_DST_COLOR,GL_ONE);
		  // thresholded image in red
		  glColor4f(1,0,0,1);
		  draw_image_once(thr[j]);
		  glBlendFunc(GL_ONE_MINUS_DST_COLOR,GL_ONE);
		  // median image in green
		  glColor4f(0,1,0,1);
		  draw_image_once(medians[j]);
	  }
      glPopMatrix();

      glDisable(GL_BLEND);
      glColor4f(1,1,1,1);

      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      {
    	  glTranslated(700, 512, 0);
    	  glScaled(12,-12,12);
          char s[100];
          sprintf(s,"%03d",j);
          glDrawText(s);
      }
      glPopMatrix();

      switch_3d(w,h);
      glPushMatrix();
      if(1){
    	  //glTranslatef(300,400,0);
    	  //glRotatef(12,0,1,1);

    	  glBegin(GL_LINES);
    	  glColor3f(1,0,0); glVertex3f(0,0,0); glVertex3f(1,0,0);
    	  glColor3f(0,1,0); glVertex3f(0,0,0); glVertex3f(0,1,0);
    	  glColor3f(0,0,1); glVertex3f(0,0,0); glVertex3f(0,0,1);
    	  glEnd();

    	  glEnable(GL_BLEND);
    	  glBlendFunc(GL_SRC_ALPHA,GL_ONE);
    	  glMatrixMode(GL_TEXTURE);
    	  glPushMatrix();
    	  {
    		  glTranslatef(.5,.5,.5);
    		  glRotatef(360.*j*1./29,0,0,1);
    		  glTranslatef(-.5,-.5,-.5);

    		  glColor4d(1,1,1,.04);
    		  draw_stack(stack);
    	  }
    	  glPopMatrix();
    	  glMatrixMode(GL_MODELVIEW);
    	  glDisable(GL_BLEND);
      }
      glPopMatrix();




      //glFlush();
      window.swap_buffers();

      usleep(32000);
      std::cin.get();

    }
  }

  return 0;
}
