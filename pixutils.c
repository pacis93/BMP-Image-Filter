#include "pixutils.h"
#include "bmp/bmp.h"

//private methods -> make static
static pixMap* pixMap_init();
static pixMap* pixMap_copy(pixMap *p);


//plugin methods <-new for Assignment 4;
static void rotate(pixMap *p, pixMap *oldPixMap,int i, int j,void *data);
static void convolution(pixMap *p, pixMap *oldPixMap,int i, int j,void *data);
static void flipVertical(pixMap *p, pixMap *oldPixMap,int i, int j,void *data);
static void flipHorizontal(pixMap *p, pixMap *oldPixMap,int i, int j,void *data);

static pixMap* pixMap_init(){
	pixMap *p=malloc(sizeof(pixMap));
	p->pixArray_overlay=0;
	return p;
}	
void pixMap_destroy (pixMap **p){
	if(!p || !*p) return ;
	pixMap *this_p=*p;
	if(this_p->pixArray_overlay)
	 free(this_p->pixArray_overlay);
	if(this_p->image)free(this_p->image);	
	free(this_p);
	this_p=0;
}
	
pixMap *pixMap_read(char *filename){
	pixMap *p=pixMap_init();
 int error;
 if((error=lodepng_decode32_file(&(p->image), &(p->imageWidth), &(p->imageHeight),filename))){
  fprintf(stderr,"error %u: %s\n", error, lodepng_error_text(error));
  return 0;
	}
	p->pixArray_overlay=malloc(p->imageHeight*sizeof(rgba*));
	p->pixArray_overlay[0]=(rgba*) p->image;
	for(int i=1;i<p->imageHeight;i++){
  p->pixArray_overlay[i]=p->pixArray_overlay[i-1]+p->imageWidth;
	}			
	return p;
}
int pixMap_write(pixMap *p,char *filename){
	int error=0;
 if(lodepng_encode32_file(filename, p->image, p->imageWidth, p->imageHeight)){
  fprintf(stderr,"error %u: %s\n", error, lodepng_error_text(error));
  return 1;
	}
	return 0;
}	 

pixMap *pixMap_copy(pixMap *p){
	pixMap *new=pixMap_init();
	*new=*p;
	new->image=malloc(new->imageHeight*new->imageWidth*sizeof(rgba));
	memcpy(new->image,p->image,p->imageHeight*p->imageWidth*sizeof(rgba));	
	new->pixArray_overlay=malloc(new->imageHeight*sizeof(void*));
	new->pixArray_overlay[0]=(rgba*) new->image;
	for(int i=1;i<new->imageHeight;i++){
  new->pixArray_overlay[i]=new->pixArray_overlay[i-1]+new->imageWidth;
	}	
	return new;
}

	
void pixMap_apply_plugin(pixMap *p,plugin *plug){
	pixMap *copy=pixMap_copy(p);
	for(int i=0;i<p->imageHeight;i++){
	 for(int j=0;j<p->imageWidth;j++){
			plug->function(p,copy,i,j,plug->data);
		}
	}
	pixMap_destroy(&copy);	 
}

int pixMap_write_bmp16(pixMap *p,char *filename){
 BMP16map *bmp16=BMP16map_init(p->imageHeight,p->imageWidth,0,5,6,5); //initialize the bmp type
 if(!bmp16) return 1;
 
	char r =5;
	char g =6;
	char b =5;
	char a = 0;
	
	for (int i = 0;i < p->imageHeight; i++) {
		for (int j = 0; j < p->imageWidth; j++) {
			int iPixel = p->imageHeight -i -1;
			int jPixel = j;
			uint16_t pix16 = 0;
			pix16 = p->pixArray_overlay[iPixel][jPixel].b >> (8-b);
			pix16 |= (p->pixArray_overlay[iPixel][jPixel].g >> (8-g)) << b;
			pix16 |= (p->pixArray_overlay[iPixel][jPixel].r >> (8-r))  << (b + g);
			pix16 |= (p->pixArray_overlay[iPixel][jPixel].a >> (8-a))  << (b + g + r);
			bmp16->pixArray[i][j]=pix16;
		}
	}

	//bmp16->pixArray[i][j] is 2-d array for bmp files. It is analogous to the one for our png file pixMaps except that it is 16 bits
	
 //However pixMap and BMP16_map are "upside down" relative to each other
 //need to flip one of the the row indices when copying

 BMP16map_write(bmp16,filename);
 BMP16map_destroy(&bmp16);
 return 0;
}	 
void plugin_destroy(plugin **plug){
 //free the allocated memory and set *plug to zero (NULL)	
 if(!plug || !*plug) return;
	plugin *this_p = *plug;
	if(this_p) free(this_p);
	this_p = 0;
}

plugin *plugin_parse(char *argv[] ,int *iptr){
	//malloc new plugin
	plugin *new=malloc(sizeof(plugin));
	new->function=0;
	new->data=0;
	
	int i=*iptr;
	if(!strcmp(argv[i]+2,"rotate")){
		new->function = rotate;
		new->data = malloc(2*sizeof(float));
		float *sc=(float*) new->data;
		int i=*iptr;
		float theta = atof(argv[i+1]);
		sc[0]=sin(degreesToRadians(-theta));
		sc[1]=cos(degreesToRadians(-theta));
		*iptr=i+2;
		return new;	
	}	
	if(!strcmp(argv[i]+2,"convolution")){
		new->function = convolution;
		new->data = malloc(9*sizeof(int));
		int (*arr)[3] =  new->data;
		int count = 1;
		for(int k = 0; k < 3; k ++) {
		for(int j = 0; j < 3; j++) {
			arr[k][j] = atoi(argv[i+count]);
		//	printf("%d\n", atoi(argv[i+count]));
			count++;
		}

	}
	
		*iptr=i+10;	
		return new;
	}
	if(!strcmp(argv[i]+2,"flipHorizontal")){
			new->function = flipHorizontal;
			
	*iptr=i+1;
  return new;
	}
	if(!strcmp(argv[i]+2,"flipVertical")){
		new->function = flipVertical;
  *iptr=i+1;
  return new;
	}		
	return(0);
}	

//define plugin functions

static void rotate(pixMap *p, pixMap *oldPixMap,int x, int y,void *data){
	float *sc = (float*) data;
	const float ox=p->imageWidth/2.0f;
	const float oy=p->imageHeight/2.0f;
	const float s=sc[0];
	const float c=sc[1];
	float rotx = c*(x-ox) - s * (oy-y) + ox;
    float roty = -(s*(x-ox) + c * (oy-y) - oy);
    int rotj=rotx+.5;
    int roti=roty+.5; 
	if(roti >=0 && roti < oldPixMap->imageHeight && rotj >=0 && rotj < oldPixMap->imageWidth){
			 
		memcpy(p->pixArray_overlay[y]+x,oldPixMap->pixArray_overlay[roti]+rotj,sizeof(rgba));			 
		}
		else{
			memset(p->pixArray_overlay[y]+x,0,sizeof(rgba));
		}

}

static void convolution(pixMap *p, pixMap *oldPixMap,int i, int j,void *data){
	int (*kernArr)[3] = data;
	int sumR = 0, sumG = 0, sumB = 0, sumA = 0;
	float kernSum = 0;
	for(int x = 0; x < 3; x++) {
		for (int y = 0; y < 3; y++) {
			kernSum +=kernArr[x][y];
		}
	}
	
	for(int kRow = -1; kRow < 2; kRow++) {
		for(int kEle = -1; kEle < 2; kEle++) {
			int kRowIdx = j +kRow;
			int kEleIdx = i +kEle;
			
			if(kRowIdx < 0) {
				kRowIdx = 0;
			} else if (kRowIdx > oldPixMap->imageHeight-1) {
				kRowIdx = oldPixMap->imageHeight-1;
			}
			if(kEleIdx < 0) {
				kEleIdx = 0;
			} else if (kEleIdx > oldPixMap->imageWidth-1) {
				kEleIdx = oldPixMap->imageWidth-1;
			}
			
			sumR += (oldPixMap->pixArray_overlay[kEleIdx][kRowIdx].r*(kernArr[kEle+1][kRow+1]))/kernSum;
			sumG += (oldPixMap->pixArray_overlay[kEleIdx][kRowIdx].g*(kernArr[kEle+1][kRow+1]))/kernSum;
			sumB += (oldPixMap->pixArray_overlay[kEleIdx][kRowIdx].b*(kernArr[kEle+1][kRow+1]))/kernSum;
			sumA += (oldPixMap->pixArray_overlay[kEleIdx][kRowIdx].a*(kernArr[kEle+1][kRow+1]))/kernSum;
		}
	}
	
	if(sumR > 255) {
		sumR = 255;
	} else if(sumR < 0) {
		sumR = 0;
	}
	if(sumG > 255) {
		sumG = 255;
	}else if(sumG < 0) {
		sumG = 0;
	}
	if(sumB > 255) {
		sumB = 255;
	}else if(sumB < 0) {
		sumB = 0;
	}
	if(sumA > 255) {
		sumA = 255;
	}else if(sumA < 0) {
		sumA = 0;
	}
			
	p->pixArray_overlay[i][j].r = sumR;
	p->pixArray_overlay[i][j].g = sumG;
	p->pixArray_overlay[i][j].b = sumB;
	p->pixArray_overlay[i][j].a = sumA;
	//implement algorithm givne in https://en.wikipedia.org/wiki/Kernel_(image_processing)
	//assume that the kernel is a 3x3 matrix of integers
	//don't forget to normalize by dividing by the sum of all the elements in the matrix
			
	
}

//very simple functions - does not use the data pointer - good place to start
 
static void flipVertical(pixMap *p, pixMap *oldPixMap,int i, int j,void *data){
 memcpy(p->pixArray_overlay[i]+j, oldPixMap->pixArray_overlay[oldPixMap->imageHeight -i -1]+j , sizeof(rgba));
}	 
static void flipHorizontal(pixMap *p, pixMap *oldPixMap,int i, int j,void *data){
 memcpy(p->pixArray_overlay[i]+j, oldPixMap->pixArray_overlay[i]+oldPixMap->imageWidth-j -1 , sizeof(rgba));
 
}
