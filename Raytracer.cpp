#include "Raytracer.h"
#include <fstream>
#include <omp.h>

#include <iostream>
using namespace std;

Raytracer::Raytracer( QString path )
{
	superSamplingRate = 1.1; //TODO: setting this to 1 and enabling on the fly output will cause a crash
	//read file and init everything

	vector<Vector> vectors;
	vector<Vector> normals;
	vector<Material> materials;
	vector<Vector> tex_coords;

	int nLights = 0, nMat =0, nVert=0, nNorm=0, nTexCoords=0, nFaces=0;

	//QString path("scene.tri");
	//create File
    ifstream file(path.toUtf8().data());

	file>>nLights>>nMat>>nVert>>nNorm>>nTexCoords>>nFaces;	//read Header

	//0.5 read Raytracer-Properties
	int width,height;
	float tx,ty,tz;	
	file>>width;file>>height;
	file>>tx; file>>ty; file>>tz;
	camera = Vector(tx,ty,tz);
	file>>tx; file>>ty;file>>tz;
	center = Vector(tx,ty,tz);
	file>>focalLength;
	float ux, uy, uz;
	file>>ux>>uy>>uz;
	upVector = Vector(ux,uy,uz);
	//file>>rotationAngle;
	float back_r,back_g,back_b;
	file>>back_r>>back_g>>back_b;
	backgroundColor.setRgb(back_r*255,back_g*255,back_b*255);
	float amb_r,amb_g,amb_b;
	file>>amb_r>>amb_g>>amb_b;
	ambientLight.setRgb(amb_r*255,amb_g*255,amb_b*255);

	image = new QImage(width*superSamplingRate, height*superSamplingRate, QImage::Format_RGB32);
	image->fill(qRgb(255,255,255));	

	//0.75 read the lights
	for (int i=0;i<nLights;++i)
	{
		Lightsource l;		
		file>>tx>>ty>>tz;
		l.position = Vector(tx,ty,tz);
		file>>l.ambient[0]>>l.ambient[1]>>l.ambient[2];
		file>>l.diffuse[0]>>l.diffuse[1]>>l.diffuse[2];
		file>>l.specular[0]>>l.specular[1]>>l.specular[2];
		file>>l.constAtt>>l.linAtt>>l.quadAtt;		
		lights.push_back(l);		
	}

	//1. Materials->Meshes
	for (int i=0;i<nMat;++i)
	{
		Material mat;
		float r,g,b,shininess,alpha,sharpness,density;
		int isTexture;
		file>>r>>g>>b;
		mat.ambient[0]=r; mat.ambient[1]=g; mat.ambient[2]=b; mat.ambient[3]=1.0;
		file>>r>>g>>b;
		mat.diffuse[0]=r; mat.diffuse[1]=g; mat.diffuse[2]=b; mat.diffuse[3]=1.0;
		file>>r>>g>>b;
		mat.specular[0]=r; mat.specular[1]=g; mat.specular[2]=b; mat.specular[3]=1.0;
		file>>shininess;
		mat.shininess = shininess;
		file>>alpha;
		mat.alpha=alpha;
		file>>sharpness;
		mat.sharpness=sharpness;
		file>>density;
		mat.density=density;
		mat.isTexture=false;
		mat.hasNormalMap = false;
		file>>isTexture;		
		if (isTexture > 0) //load texture
		{
			string texName;
			file>>texName;
			cout<<"Texture: "<<texName<<endl;
			QString filepath; //path in were the
			filepath = path;
			filepath.chop(path.length()-path.lastIndexOf("/"));
			filepath.append(QString("/%1").arg(QString(texName.c_str())));
			QImage texture(filepath);
			if(!texture.isNull())
			{
				mat.texture = texture;
				mat.isTexture = true;
			}
			else
				QMessageBox::warning(this, tr("Texture Loader"), tr("Texture couldn't be loaded!"),QMessageBox::Ok);			
		}
		if (isTexture == 2) //load normal map (heavy code duplicates TODO: change that)
		{
			string bumpName;
			file>>bumpName;
			cout<<"Normal Map: "<<bumpName<<endl;
			QString filepath; //path in were the
			filepath = path;
			filepath.chop(path.length()-path.lastIndexOf("/"));
			filepath.append(QString("/%1").arg(QString(bumpName.c_str())));
			QImage bumpMap(filepath);
			if(!bumpMap.isNull())
			{
				mat.hasNormalMap = true;
				mat.normalMap = bumpMap;
			}
			else
				QMessageBox::warning(this, tr("Texture Loader"), tr("Normal map couldn't be loaded!"),QMessageBox::Ok);
		}
		materials.push_back(mat);
	}
	//2.Fill vertices
	for (int i=0;i<nVert;++i)
	{
		float x,y,z;
		file>>x>>y>>z;
		vectors.push_back(Vector(x,y,z));
	}
	//3.Fill normals
	for (int i=0;i<nNorm;++i)
	{
		float x,y,z;
		file>>x>>y>>z;
		Vector n(x,y,z);
		n.normalize();
		normals.push_back(n);
	}
	//3.Fill texCoords
	for (int i=0;i<nTexCoords;++i)
	{
		float u,v;
		file>>u>>v;
		tex_coords.push_back(Vector(u,v,1.0));
	}
	//4.Fill faces /Triangles
	for (int i=0;i<nFaces;++i)
	{
		int matNr,vertNr1,vertNr2,vertNr3,NormNr1,NormNr2,NormNr3,TexCoordsNr1,TexCoordsNr2,TexCoordsNr3;
		file>>matNr>>vertNr1>>vertNr2>>vertNr3>>NormNr1>>NormNr2>>NormNr3>>TexCoordsNr1>>TexCoordsNr2>>TexCoordsNr3;
		Triangle t;
		//cout<<"Size: "<<materials.size()<<", Nr: "<<matNr<<endl;cout.flush();
		t.material = materials[matNr];
		t.vertices[0]=vectors[vertNr1];
		t.vertices[1]=vectors[vertNr2];
		t.vertices[2]=vectors[vertNr3];
		t.normals[0]=normals[NormNr1];
		t.normals[1]=normals[NormNr2];
		t.normals[2]=normals[NormNr3];
		if (materials[matNr].isTexture)
		{
			t.texCoords[0]=tex_coords[TexCoordsNr1];
			t.texCoords[1]=tex_coords[TexCoordsNr2];
			t.texCoords[2]=tex_coords[TexCoordsNr3];
		}
		triangles.push_back(t);
	}
	cout<<"Got "<<triangles.size()<<" Triangles\n";

}

void Raytracer::init()
{
	//draw the geometry into a texture and save the primary-Rays
	GLubyte *renderedImage = new GLubyte[image->height()*image->width()*4];
	
	gluLookAt(camera[0], camera[1], camera[2], center[0], center[1], center[2], upVector[0], upVector[1], upVector[2]);
	setGeometry(0,0,1024,768);
	makeCurrent();
	GLenum err = glewInit();
    if (GLEW_OK != err)
        cout << "Error: glewInit() failed\n";
    else
        cout << "Succesfully initiated GLEW\n";

	
	//create DisplayList
	displayList = glGenLists(1);
	glNewList(displayList, GL_COMPILE);
	for (int i=0; i<triangles.size(); ++i)
	{
		float r = (float)(i % 255);
		float g = (float)((i/255) % 255);
		float b = (float)(((i/255)/255) % 255);
		float a = (float)(((((i/255)/255))/255) % 255);	
		glColor4f(r/255.0,g/255.0,b/255.0,a/255.0);
		glBegin(GL_TRIANGLES);
		glVertex3fv( triangles[i].vertices[0].getValues() );
		glVertex3fv( triangles[i].vertices[1].getValues() );
		glVertex3fv( triangles[i].vertices[2].getValues() );
		glEnd();
	}
	glEndList();

	
	glGenTextures(1, &screenTexID);
	glBindTexture(GL_TEXTURE_2D,screenTexID);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, 4, image->width(), image->height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	// create a renderbuffer object to store depth info
	GLuint rboId;
	glGenRenderbuffersEXT(1, &rboId);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, rboId);
	glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT, image->width(), image->height());
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
	
	//GLuint fboId;
	glGenFramebuffersEXT(1, &fboId);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,fboId);	

	// attach the texture to FBO color attachment point
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, screenTexID, 0);
	// attach the renderbuffer to depth attachment point
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, rboId);
	
	resizeGL(image->width(), image->height());
	
	paintGL();	
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	
	glBindTexture(GL_TEXTURE_2D, screenTexID);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, (void*)renderedImage);

	
	idx = new unsigned int[image->height()*image->width()];

	for (int i=0; i<image->height()*image->width()*4; i+=4)
		idx[i/4] = (int)renderedImage[i] + (int)(renderedImage[i+1])*255 + (int)(renderedImage[i+2])*255*255 + (int)(renderedImage[i+3])*255*255*255;

	delete renderedImage;

}

void Raytracer::resizeGL(int w, int h)
{
	// Reset the viewport
    glViewport(0, 0, w, h);
    // Reset the projection and modelview matrix
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
	gluPerspective(45.0, (GLfloat)w/(GLfloat)h, 1.0f, 200.0f);
    glMatrixMode(GL_MODELVIEW);
}

void Raytracer::initializeGL()
{
	glClearColor((float)backgroundColor.red()/255.0, (float)backgroundColor.green()/255.0, (float)backgroundColor.blue()/255.0, 1.0);
	//glClearColor(1.0, 1.0, 1.0, 0.0);
	cout<<"Back_Color: "<<backgroundColor.red()/255.0<<", "<<(float)backgroundColor.green()/255.0<<", "<<(float)backgroundColor.blue()/255.0<<endl;
	
	glDepthFunc(GL_LEQUAL);							// Type Of Depth Testing
	glEnable(GL_DEPTH_TEST);						// Enable Depth Testing	
}


void Raytracer::paintGL()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	
	glDisable(GL_CULL_FACE);
	glEnable(GL_COLOR_MATERIAL);
	glCallList(displayList);
}

void Raytracer::genImage()
{
	QTime t;
	t.start();
	
	init();

	int count = 0;

	unsigned int backgroundCode = backgroundColor.red() + backgroundColor.green()*255 + backgroundColor.blue()*255*255 + (unsigned int)((unsigned int)255*(unsigned int)255*(unsigned int)255*(unsigned int)255); //the alpha value of background is always set  to 1.0 (255)
	cout<<"Back_Code: "<<backgroundCode<<endl;
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,fboId);	
	paintGL();
	GLdouble mvMatrix[16];
	GLdouble projMatrix[16];
	int viewPort[4];
	float* zValues = new float[image->height()*image->width()];
	glReadPixels( 0, 0, image->width(), image->height(), GL_DEPTH_COMPONENT, GL_FLOAT, zValues );
	glGetIntegerv(GL_VIEWPORT, viewPort);
	glGetDoublev(GL_MODELVIEW_MATRIX,mvMatrix);
	glGetDoublev(GL_PROJECTION_MATRIX,projMatrix);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,0);

	QWidget *w = new QWidget(NULL);
	QLabel *label = new QLabel(w);
	label->setGeometry(0,0,image->width()/superSamplingRate, image->height()/superSamplingRate);
	w->resize(label->size());
	w->setWindowTitle(QString("Rendered image"));
	w->show();
	

#pragma omp parallel for schedule(dynamic, 1)
  for (int j=0; j<image->height(); j+=1)
	{	
		for (int i=0; i<image->width(); i+=1)
		{
			unsigned int index = idx[j*image->width()+i];
			QColor c;
			if (index != backgroundCode)
			{
				double dX=0.0, dY=0.0, dZ=0.0;
				gluUnProject( (float)i, (float)j, zValues[j*image->width()+i], mvMatrix, projMatrix, viewPort, &dX, &dY, &dZ );
				Vector intersection(dX, dY, dZ);
				Vector dir = intersection - camera;
				c = raytrace(camera, dir, MAX_DEPTH);
                                				
			}
      		else
        		c = backgroundColor;

			image->setPixel(i,image->height()-(j+1), QRgb(c.rgb()));

		}

		#pragma omp critical
		{
			++count;
			cout<<"\r----"<<(float)count/(float)image->height()*100.0<<"----";
		}
		
		#pragma omp master
		{
			if (!(superSamplingRate <= 1.0) || omp_get_num_threads()==1)
			{
				label->setPixmap(QPixmap::fromImage(image->scaled ( image->width()/superSamplingRate, image->height()/superSamplingRate, Qt::IgnoreAspectRatio, Qt::FastTransformation )));
				label->repaint();
			}
		}

	}
	delete zValues;
	cout<<endl;

	label->setPixmap(QPixmap::fromImage(image->scaled ( image->width()/superSamplingRate, image->height()/superSamplingRate, Qt::IgnoreAspectRatio, Qt::SmoothTransformation )));
	label->repaint();
	cout<<"Time: "<<t.elapsed()/1000.0<<endl;

    updateGL();
}


QColor Raytracer::raytrace(Vector start, Vector dir, int depth)
{
    // t ist der Parameter der Geradengleichung f�r unseren Strahl. Er sagt uns, wie weit das geschnittene Dreieck vom "Auge" entfernt ist.
    float t = -1;

    // speichert immer nur den kleinsten Parameter t der Geraden Gleichung, der das dem Auge am n�chsten befindliche Dreieck beschreibt, alle gr��eren Dreiecke f�r ein t, werden vom n�hsten verdeckt.
    float kleinstesT;
    QColor aktuelleFarbe;
	
    // Variable soll Wert 1 haben, falls der Strahl schon einmal ein Dreieck geschnitten hat. 0 sonst
    int hatsSchonmalGeschnitten = 0;
	
    // Durchlaufe alle Dreiecke der Szene
    for(int i = 0; i < triangles.size(); i+=1)
    {
        // Berechne Parameter t der Geraden, f�r dieses Dreieck, setze t negativ, wenn b*n = 0.
        if((scalarProduct(dir, triangles.at(i).planeNormal)) != 0)
            t = (scalarProduct((triangles.at(i).vertices[0] - start), triangles.at(i).planeNormal)) / scalarProduct(dir, triangles.at(i).planeNormal);
        else t = -1;

        // teste, ob ein Schnittpunkt mit dem Strahl und dem aktuellen Dreieck besteht
        if((gibtsSchnittpunkt(start, dir, triangles.at(i), t)) == 1)
        {
            // Hat der Strahl bisher noch kein Dreieck geschnitten, ist dies der erste erfolgreiche Schnitt, also ist dies das kleinste t.
            if(hatsSchonmalGeschnitten == 0)
            {				
                hatsSchonmalGeschnitten = 1;
                kleinstesT = t;
                // TODO gib eine sinnvolle Farbe zur�ck.
                aktuelleFarbe = QColor(254,0,0);
            }
            // sonst bestimme kleinstes t, durch vergleich
            else
            {
                // TODO gib eine sinnvolle Farbe zur�ck.
                if(kleinstesT > t) aktuelleFarbe = QColor(254,0,0);
            }
        }
    }
	
    // gib hintergrundfarbe zur�ck, wenn kein Dreieck geschnitten wurde
    if(hatsSchonmalGeschnitten == 0) return backgroundColor;
    // sonst die bestimmte farbe
    else return aktuelleFarbe;
}

int Raytracer::gibtsSchnittpunkt(Vector start, Vector dir, Triangle tri, float t)
{
    // wenn t negativ, liegt das dreieck hinter dem "Auge", also kann falsch zur�ck gegeben werden
    if(t < 0) return 0;
    // berechne den Punkt, in dem der Strahl, die Ebene des Dreiecks scheidet.
    Vector p; //= start + crossProduct(Vector(t,t,t,1), dir);

    p.setValue(0, start.getValues()[0] + t*dir.getValues()[0]);
    p.setValue(1, start.getValues()[1] + t*dir.getValues()[1]);
    p.setValue(2, start.getValues()[2] + t*dir.getValues()[2]);
    p.setValue(3, start.getValues()[3] + t*dir.getValues()[3]);

    float areaTri = 0.5 * (crossProduct((tri.vertices[1] - tri.vertices[0]),(tri.vertices[3] - tri.vertices[0]))).norm();
    float alpha1 = (0.5 * (crossProduct((tri.vertices[1] - p),tri.vertices[2] - p)).norm()) / areaTri;
    float alpha2 = (0.5 * (crossProduct((tri.vertices[2] - p),tri.vertices[0] - p)).norm()) / areaTri;
    float alpha3 = (0.5 * (crossProduct((tri.vertices[0] - p),tri.vertices[1] - p)).norm()) / areaTri;

    // wenn der Punkt ausserhalb des Dreiecks liegt.
    if(alpha1 >= 0 && alpha2 >= 0 && alpha3 >= 0)
    {
        // 1 soll bedeuten, dass der Strahl das Dreieck schneidet.
        return 1;
    }
    // null soll bedeuten, dass der Strahl das Dreieck nicht schneidet.
    return 0;
}


void Raytracer::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_S)
	{
		QString path = QFileDialog::getSaveFileName ( NULL, QString("Save Image"));
		finalImage.save ( path, "PNG", -1 );
	}
}

