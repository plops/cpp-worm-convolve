#include <cvd/image_io.h>
#include <cvd/videodisplay.h> 
#include <cvd/gl_helpers.h>  
#include <cvd/draw.h> 
#include <cvd/morphology.h>
#include <cvd/connected_components.h>

using namespace CVD;
using namespace std;
/* confocal test data: /huge/local/martin/d0318/nikon-a1r/image2.ids
thereof 22 slices are nice
dz=604.8 nm
layout  order   bits    ch      x       y       z
layout  sizes   16      1       512     512     29
representation  format  integer
representation  sign    unsigned
representation  byte_order      1       2
parameter       origin  0.0     0.0     0.000000        0.000000        8.770000
parameter       scale   0.0     0.0     0.621481        0.621481        0.604828
parameter       range   1.0     1.000000        318.198052      318.198052      17.540000
parameter       units   relative        undefined       um      um      um
parameter       ch      EGFP
sensor  s_params        pinholeradius   42.145594
sensor  s_params        lambdaex        488.000000
sensor  s_params        lambdaem        525.000000
sensor  s_params        refrinxmedium   1.515000
sensor  s_params        numaperture     1.300000
sensor  s_params        refrinxlensmedium       1.515000

taken stuff from
git://github.com/paulreimer/ofxMarsyasApps.git
(in google codesearch)

and libcvd/test/qfloodfill_test.cc
for color scaling:
0215/tif.lisp
 */

Image<byte> threshold(Image<unsigned short>in,int thresh)
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


Image<byte> find_components(Image<byte>in) // poor mans implementation of optional parameter
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

void
draw_image(Image<byte>&temp)
{
  glEnable(GL_TEXTURE_RECTANGLE_NV);
  //glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  glTexParameterf( GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
  glTexParameterf( GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
  glPixelStorei(GL_UNPACK_ALIGNMENT,1);
  glTexImage2D(temp, 0, GL_TEXTURE_RECTANGLE_NV);
  glBegin(GL_QUADS);
  int x=temp.size().x, y=temp.size().y;
  glTexCoord2i(0, 0);
  glVertex2i(0,0);
  glTexCoord2i(x, 0);
  glVertex2i(x,0);
  glTexCoord2i(x,y);
  glVertex2i(x,y);
  glTexCoord2i(0,y);
  glVertex2i(0,y);
  glEnd ();
  glDisable(GL_TEXTURE_RECTANGLE_NV);
}

int main()
{
  vector<Image<unsigned short> > in;
  load_ids("/huge/local/martin/d0318/nikon-a1r/image2.ids",in);
  
  vector<Image<byte> > comps,thr,medians;
  vector<ImageRef> element=getDisc(2);

  for(int i=0;i<in.size();i++){
    thr.push_back(threshold(in[i],600));
    Image<byte> median(thr[0].size()); 
    morphology(thr[thr.size()-1], element, Morphology::Median<byte>(), median);
    medians.push_back(median);
    comps.push_back(find_components(median));
  }

  VideoDisplay window(ImageRef(1024,700)); 

  for(int c=0;c<100;c++){
    for(int j=0;j<29;j++){
      glClear(GL_COLOR_BUFFER_BIT);
      glRasterPos2f(0,0);

      glMatrixMode(GL_COLOR);
      glPushMatrix();
      glScalef(30,30,30);
      glDrawPixels(in[j]);
      glPopMatrix();
      
      glRasterPos2f(512,0);
      glDrawPixels(comps[j]);

      // superimpose some images
      glEnable(GL_BLEND);
      glBlendFunc(GL_DST_COLOR,GL_ONE);
      // thresholded image in red
      glColor4f(1,0,0,1);
      draw_image(thr[j]);
      glBlendFunc(GL_ONE_MINUS_DST_COLOR,GL_ONE);
      // median image in green
      glColor4f(0,1,0,1);
      draw_image(medians[j]);
      
      glDisable(GL_BLEND);
      glColor4f(1,1,1,1);
      
      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      glTranslated(512, 512, 0);
      glScaled(12,-12,12);
      char s[100];
      sprintf(s,"%03d",j);
      glDrawText(s);
      glPopMatrix();

      glFlush();
      //window.swap_buffers();
      
      usleep(32000);
      std::cin.get();
      
    }
  }
  
 

  
  return 0;
}
