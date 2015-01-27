// CS559 Train Project
// TrainView class implementation
// see the header for details
// look for TODO: to see things you want to add/change
// 

#include "TrainView.H"
#include "TrainWindow.H"

#include "Utilities/3DUtils.H"

#include <Fl/fl.h>

// we will need OpenGL, and OpenGL needs windows.h
#include <windows.h>
#include "GL/gl.h"
#include "GL/glu.h"
#include "glm/glm.hpp"

#define PI 3.14159265

float m_pointPosition = 0; // position for train movement

#ifdef EXAMPLE_SOLUTION
#include "TrainExample/TrainExample.H"
#endif


TrainView::TrainView(int x, int y, int w, int h, const char* l) : Fl_Gl_Window(x, y, w, h, l),
m_pointPosition(0.0),
m_trainWidth(20.0f),
m_passengerWidth(20.0f),
m_cabooseWidth(10.0f)
{
    mode(FL_RGB | FL_ALPHA | FL_DOUBLE | FL_STENCIL);

    resetArcball();
}

void TrainView::resetArcball()
{
    // set up the camera to look at the world
    // these parameters might seem magical, and they kindof are
    // a little trial and error goes a long way
    arcball.setup(this, 40, 250, .2f, .4f, 0);
}

// FlTk Event handler for the window
// TODO: if you want to make the train respond to other events 
// (like key presses), you might want to hack this.
int TrainView::handle(int event)
{
    // see if the ArcBall will handle the event - if it does, then we're done
    // note: the arcball only gets the event if we're in world view
    if (tw->worldCam->value())
        if (arcball.handle(event)) return 1;

    // remember what button was used
    static int last_push;

    switch (event) {
    case FL_PUSH:
        last_push = Fl::event_button();
        if (last_push == 1) {
            doPick();
            damage(1);
            return 1;
        };
        break;
    case FL_RELEASE:
        damage(1);
        last_push = 0;
        return 1;
    case FL_DRAG:
        if ((last_push == 1) && (selectedCube >= 0)) {
            ControlPoint* cp = &world->points[selectedCube];

            double r1x, r1y, r1z, r2x, r2y, r2z;
            getMouseLine(r1x, r1y, r1z, r2x, r2y, r2z);

            double rx, ry, rz;
            mousePoleGo(r1x, r1y, r1z, r2x, r2y, r2z,
                static_cast<double>(cp->pos.x),
                static_cast<double>(cp->pos.y),
                static_cast<double>(cp->pos.z),
                rx, ry, rz,
                (Fl::event_state() & FL_CTRL) != 0);
            cp->pos.x = (float)rx;
            cp->pos.y = (float)ry;
            cp->pos.z = (float)rz;
            damage(1);
        }
        break;
        // in order to get keyboard events, we need to accept focus
    case FL_FOCUS:
        return 1;
    case FL_ENTER:	// every time the mouse enters this window, aggressively take focus
        focus(this);
        break;
    case FL_KEYBOARD:
        int k = Fl::event_key();
        int ks = Fl::event_state();
        if (k == 'p') {
            if (selectedCube >= 0)
                printf("Selected(%d) (%g %g %g) (%g %g %g)\n", selectedCube,
                world->points[selectedCube].pos.x, world->points[selectedCube].pos.y, world->points[selectedCube].pos.z,
                world->points[selectedCube].orient.x, world->points[selectedCube].orient.y, world->points[selectedCube].orient.z);
            else
                printf("Nothing Selected\n");
            return 1;
        };
        break;
    }

    return Fl_Gl_Window::handle(event);
}

// this is the code that actually draws the window
// it puts a lot of the work into other routines to simplify things
void TrainView::draw()
{

    glViewport(0, 0, w(), h());

    // clear the window, be sure to clear the Z-Buffer too
    glClearColor(0, 0, .3f, 0);		// background should be blue
    // we need to clear out the stencil buffer since we'll use
    // it for shadows
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glEnable(GL_DEPTH);

    // Blayne prefers GL_DIFFUSE
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    // prepare for projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    setProjection();		// put the code to set up matrices here

    // TODO: you might want to set the lighting up differently
    // if you do, 
    // we need to set up the lights AFTER setting up the projection

    // enable the lighting
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    // top view only needs one light
    if (tw->topCam->value()) {
        glDisable(GL_LIGHT1);
        glDisable(GL_LIGHT2);
    }
    else {
        glEnable(GL_LIGHT1);
        glEnable(GL_LIGHT2);
    }
    // set the light parameters
    GLfloat lightPosition1[] = { 0, 1, 1, 0 }; // {50, 200.0, 50, 1.0};
    GLfloat lightPosition2[] = { 1, 0, 0, 0 };
    GLfloat lightPosition3[] = { 0, -1, 0, 0 };
    GLfloat yellowLight[] = { 0.5f, 0.5f, .1f, 1.0 };
    GLfloat whiteLight[] = { 1.0f, 1.0f, 1.0f, 1.0 };
    GLfloat blueLight[] = { .1f, .1f, .3f, 1.0 };
    GLfloat grayLight[] = { .3f, .3f, .3f, 1.0 };

    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition1);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, whiteLight);
    glLightfv(GL_LIGHT0, GL_AMBIENT, grayLight);

    glLightfv(GL_LIGHT1, GL_POSITION, lightPosition2);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, yellowLight);

    glLightfv(GL_LIGHT2, GL_POSITION, lightPosition3);
    glLightfv(GL_LIGHT2, GL_DIFFUSE, blueLight);



    // now draw the ground plane
    if (!tw->trainCam->value()) {
        setupFloor();
        glDisable(GL_LIGHTING);
        drawFloor(200, 10);
        glEnable(GL_LIGHTING);
        setupObjects();
    }
    else {
        setupFloor();
        setupObjects();
    }

    // we draw everything twice - once for real, and then once for
    // shadows
    drawStuff();

    // this time drawing is for shadows (except for top view)
    if (!tw->topCam->value()) {
        setupShadows();
        drawStuff(true);
        unsetupShadows();
    }

}

// note: this sets up both the Projection and the ModelView matrices
// HOWEVER: it doesn't clear the projection first (the caller handles
// that) - its important for picking
void TrainView::setProjection()
{
    // compute the aspect ratio (we'll need it)
    float aspect = static_cast<float>(w()) / static_cast<float>(h());

    if (tw->worldCam->value())
        arcball.setProjection(false);
    else if (tw->topCam->value()) {
        float wi, he;
        if (aspect >= 1) {
            wi = 110;
            he = wi / aspect;
        }
        else {
            he = 110;
            wi = he*aspect;
        }
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(-wi, wi, -he, he, -200, 200);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glRotatef(90, 1, 0, 0);
    }
    else {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        // Compute the aspect ratio so we don't distort things
        double aspect = ((double)w()) / ((double)h());
        gluPerspective(40, aspect, .1, 1000);

        // Put the camera where we want it to be
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }
}

// this draws all of the stuff in the world
// NOTE: if you're drawing shadows, DO NOT set colors 
// (otherwise, you get colored shadows)
// this gets called twice per draw - once for the objects, once for the shadows
// TODO: if you have other objects in the world, make sure to draw them
void TrainView::drawStuff(bool doingShadows)
{
    // draw the control points
    // don't draw the control points if you're driving 
    // (otherwise you get sea-sick as you drive through them)
    if (!tw->trainCam->value()) {
        for (size_t i = 0; i<world->points.size(); ++i) {
            if (!doingShadows) {
                if (((int)i) != selectedCube)
                    glColor3ub(240, 60, 60);
                else
                    glColor3ub(240, 240, 30);
            }
            world->points[i].draw();
        }
    }
    // TODO: call your own track drawing code
    vector<Pnt3f> interpolatedPoints;
    vector<Pnt3f> offsetPoints;
    int splineType = tw->splineBrowser->value();
    drawTrack(this, doingShadows, splineType, interpolatedPoints, offsetPoints);
    // draw the track
    drawScenery(doingShadows);

    if (!tw->trainCam->value()) {
        drawTrain(this, doingShadows, interpolatedPoints, offsetPoints);
    }

#ifdef EXAMPLE_SOLUTION
    drawTrack(this, doingShadows);
#endif

    // draw the train
    // TODO: call your own train drawing code
    // train draw is called from track function.

#ifdef EXAMPLE_SOLUTION
    // don't draw the train if you're looking out the front window
    if (!tw->trainCam->value())
        drawTrain(this, doingShadows);
#endif
}
//Author Steven Volocyk
void TrainView::positionCameraInTrain(std::vector<Pnt3f>& railOnePoints, std::vector<Pnt3f>& railTwoPoints)
{
    float height, depth, xTranslation, yTranslation, zTranslation;
    Pnt3f tmpPoint, tmpPoint2;
    int point = (int)m_pointPosition;
    float percentage = m_pointPosition - point;
    double angleOfRotation;
    height = 3;
    depth = 4;
    point = point % railOnePoints.size(); // Ensure we roll over
    float pasPos = 0;
    float cabPos = 0;

    size_t nextIndex = point + 1;
    if (nextIndex >= railOnePoints.size()) {
        nextIndex = 0;
    }

    tmpPoint.x = railOnePoints[point].x;
    tmpPoint.y = railOnePoints[point].y;
    tmpPoint.z = railOnePoints[point].z;

    tmpPoint2.x = railOnePoints[nextIndex].x;
    tmpPoint2.y = railOnePoints[nextIndex].y;
    tmpPoint2.z = railOnePoints[nextIndex].z;

    angleOfRotation = -atan2(tmpPoint2.z - tmpPoint.z, tmpPoint2.x - tmpPoint.x) * 180 / PI;
    double zAngle = 0.0;
    zAngle = -atan2(tmpPoint2.y - tmpPoint.y, abs(tmpPoint2.z - tmpPoint.z)) * 180 / PI;

    tmpPoint.x = (railOnePoints[nextIndex].x - railOnePoints[point].x) * percentage + railOnePoints[point].x;
    tmpPoint.y = (railOnePoints[nextIndex].y - railOnePoints[point].y) * percentage + railOnePoints[point].y;
    tmpPoint.z = (railOnePoints[nextIndex].z - railOnePoints[point].z) * percentage + railOnePoints[point].z;

    tmpPoint2.x = (railTwoPoints[nextIndex].x - railTwoPoints[point].x) * percentage + railTwoPoints[point].x;
    tmpPoint2.y = (railTwoPoints[nextIndex].y - railTwoPoints[point].y) * percentage + railTwoPoints[point].y;
    tmpPoint2.z = (railTwoPoints[nextIndex].z - railTwoPoints[point].z) * percentage + railTwoPoints[point].z;

    xTranslation = (tmpPoint2.x + tmpPoint.x) / 2;
    yTranslation = tmpPoint.y;
    zTranslation = (tmpPoint.z + tmpPoint2.z) / 2;

    tmpPoint.x -= railOnePoints[point].x;
    tmpPoint.y -= railOnePoints[point].y;
    tmpPoint.z -= railOnePoints[point].z;
    tmpPoint.normalize();
    gluLookAt(xTranslation, yTranslation + 7.5, zTranslation, xTranslation + tmpPoint.x * 35, yTranslation + tmpPoint.y * 15,
        zTranslation + tmpPoint.z * 35, 0, 1, 0);

    glDisable(GL_LIGHTING);
    drawFloor(200, 10);
    glEnable(GL_LIGHTING);
}


//Author Steven Volocyk
//handles drawing the train positioning and movement via the sets of points generated by drawTrack.
// draws the caboose, passenger cars, and train engine.
void TrainView::drawTrain(TrainView* arg, bool doingShadows, vector<Pnt3f>& railOnePoints, vector<Pnt3f>& railTwoPoints) {
    float height, depth, xTranslation, yTranslation, zTranslation;
    Pnt3f tmpPoint, tmpPoint2;
    int point = (int)m_pointPosition;
    float percentage = m_pointPosition - point;
    double angleOfRotation;
    height = 3;
    depth = 4;
    point = point % railOnePoints.size(); // Ensure we roll over
    float pasPos = 0;
    float cabPos = 0;

    size_t nextIndex = point + 1;
    if (nextIndex >= railOnePoints.size()) {
        nextIndex = 0;
    }

    tmpPoint.x = railOnePoints[point].x;
    tmpPoint.y = railOnePoints[point].y;
    tmpPoint.z = railOnePoints[point].z;

    tmpPoint2.x = railOnePoints[nextIndex].x;
    tmpPoint2.y = railOnePoints[nextIndex].y;
    tmpPoint2.z = railOnePoints[nextIndex].z;

    angleOfRotation = -atan2(tmpPoint2.z - tmpPoint.z, tmpPoint2.x - tmpPoint.x) * 180 / PI;
    double zAngle = 0.0;
    zAngle = -atan2(tmpPoint2.y - tmpPoint.y, abs(tmpPoint2.z - tmpPoint.z)) * 180 / PI;

    tmpPoint.x = (railOnePoints[nextIndex].x - railOnePoints[point].x) * percentage + railOnePoints[point].x;
    tmpPoint.y = (railOnePoints[nextIndex].y - railOnePoints[point].y) * percentage + railOnePoints[point].y;
    tmpPoint.z = (railOnePoints[nextIndex].z - railOnePoints[point].z) * percentage + railOnePoints[point].z;

    tmpPoint2.x = (railTwoPoints[nextIndex].x - railTwoPoints[point].x) * percentage + railTwoPoints[point].x;
    tmpPoint2.y = (railTwoPoints[nextIndex].y - railTwoPoints[point].y) * percentage + railTwoPoints[point].y;
    tmpPoint2.z = (railTwoPoints[nextIndex].z - railTwoPoints[point].z) * percentage + railTwoPoints[point].z;

    xTranslation = (tmpPoint2.x + tmpPoint.x) / 2;
    yTranslation = tmpPoint.y;
    zTranslation = (tmpPoint.z + tmpPoint2.z) / 2;

    float xDelta = (railOnePoints[nextIndex].x - railOnePoints[point].x) * percentage;
    float yDelta = (railOnePoints[nextIndex].y - railOnePoints[point].y) * percentage;
    float zDelta = (railOnePoints[nextIndex].z - railOnePoints[point].z) * percentage;

    float delta = sqrt(xDelta * xDelta + yDelta * yDelta + zDelta * zDelta);
    float magnitude = 0.0f;
    double a, b, c;
    Pnt3f prevPoint, currPoint;

    float xPassTrans = 0.0f, yPassTrans = 0.0f, zPassTrans = 0.0f;
    float passengerAngle = 0.0;
    float passengerZAngle = 0.0;
    float desiredOffset = m_trainWidth + 1;
    while (true)
    {
        if (point < 0) {
            point = railOnePoints.size() - 1;
        }
        currPoint = railOnePoints[point];

        int nextIndex = point - 1;
        if (nextIndex < 0) {
            nextIndex = railOnePoints.size() - 1;
        }
        prevPoint = railOnePoints[nextIndex];

        a = (double)currPoint.x - prevPoint.x;
        b = (double)currPoint.y - prevPoint.y;
        c = (double)currPoint.z - prevPoint.z;

        magnitude = (float)sqrt(a*a + b*b + c*c);

        if (delta + magnitude >= desiredOffset) {
            float amountTraveled = desiredOffset - delta;
            float percentage = amountTraveled / magnitude;

            xPassTrans = currPoint.x - (currPoint.x - prevPoint.x) * percentage;
            yPassTrans = currPoint.y - (currPoint.y - prevPoint.y) * percentage;
            zPassTrans = currPoint.z - (currPoint.z - prevPoint.z) * percentage;

            tmpPoint.x = currPoint.x - (currPoint.x - prevPoint.x) * percentage;
            tmpPoint.y = currPoint.y - (currPoint.y - prevPoint.y) * percentage;
            tmpPoint.z = currPoint.z - (currPoint.z - prevPoint.z) * percentage;

            tmpPoint2.x = railTwoPoints[point].x - (railTwoPoints[point].x - railTwoPoints[nextIndex].x) * percentage;
            tmpPoint2.y = railTwoPoints[point].y - (railTwoPoints[point].y - railTwoPoints[nextIndex].y) * percentage;
            tmpPoint2.z = railTwoPoints[point].z - (railTwoPoints[point].z - railTwoPoints[nextIndex].z) * percentage;

            passengerAngle = -atan2(zPassTrans - prevPoint.z, xPassTrans - prevPoint.x) * 180 / PI;
            passengerZAngle = atan2(yPassTrans - prevPoint.y, abs(zPassTrans - prevPoint.z)) * 180 / PI;

            xPassTrans = (tmpPoint2.x + tmpPoint.x) / 2;
            yPassTrans = tmpPoint.y;
            zPassTrans = (tmpPoint.z + tmpPoint2.z) / 2;

            break;
        }
        else {
            delta += magnitude;
        }
        point--;
    }

    float cabooseAngle = 0.0;
    float cabooseZAngle = 0.0;

    while (true)
    {
        if (point < 0) {
            point = railOnePoints.size() - 1;
        }
        currPoint = railOnePoints[point];

        int nextIndex = point - 1;
        if (nextIndex < 0) {
            nextIndex = railOnePoints.size() - 1;
        }
        prevPoint = railOnePoints[nextIndex];

        a = (double)currPoint.x - prevPoint.x;
        b = (double)currPoint.y - prevPoint.y;
        c = (double)currPoint.z - prevPoint.z;

        magnitude = (float)sqrt(a*a + b*b + c*c);

        if (delta + magnitude >= desiredOffset) {
            float amountTraveled = desiredOffset - delta;
            float percentage = amountTraveled / magnitude;

            xPassTrans = currPoint.x - (currPoint.x - prevPoint.x) * percentage;
            yPassTrans = currPoint.y - (currPoint.y - prevPoint.y) * percentage;
            zPassTrans = currPoint.z - (currPoint.z - prevPoint.z) * percentage;

            tmpPoint.x = currPoint.x - (currPoint.x - prevPoint.x) * percentage;
            tmpPoint.y = currPoint.y - (currPoint.y - prevPoint.y) * percentage;
            tmpPoint.z = currPoint.z - (currPoint.z - prevPoint.z) * percentage;

            tmpPoint2.x = railTwoPoints[point].x - (railTwoPoints[point].x - railTwoPoints[nextIndex].x) * percentage;
            tmpPoint2.y = railTwoPoints[point].y - (railTwoPoints[point].y - railTwoPoints[nextIndex].y) * percentage;
            tmpPoint2.z = railTwoPoints[point].z - (railTwoPoints[point].z - railTwoPoints[nextIndex].z) * percentage;

            passengerAngle = -atan2(zPassTrans - prevPoint.z, xPassTrans - prevPoint.x) * 180 / PI;
            passengerZAngle = atan2(yPassTrans - prevPoint.y, abs(zPassTrans - prevPoint.z)) * 180 / PI;

            xPassTrans = (tmpPoint2.x + tmpPoint.x) / 2;
            yPassTrans = tmpPoint.y;
            zPassTrans = (tmpPoint.z + tmpPoint2.z) / 2;

            break;
        }
        else {
            delta += magnitude;
        }
        point--;
    }

    glPushMatrix();
    {
        glTranslatef(xTranslation, yTranslation + 4.0f, zTranslation);
        glRotated(angleOfRotation + 180, 0, 1, 0);
        glRotated(zAngle, 0, 0, 1);
        drawLocoBody(m_trainWidth, height, depth, doingShadows);
    }
    glPopMatrix();

    glPushMatrix();
    {
        glTranslatef(xPassTrans, yPassTrans + 4.0f, zPassTrans);
        glRotated(passengerAngle, 0, 1, 0);
        glRotated(passengerZAngle, 0, 0, 1);
        drawPassengerCar(m_passengerWidth, 3.0f, 4.0f, doingShadows);
    }
    glPopMatrix();

    //drawCaboose

    while (true)
    {
        if (point < 0) {
            point = railOnePoints.size() - 1;
        }
        currPoint = railOnePoints[point];

        int nextIndex = point - 1;
        if (nextIndex < 0) {
            nextIndex = railOnePoints.size() - 1;
        }
        prevPoint = railOnePoints[nextIndex];

        a = (double)currPoint.x - prevPoint.x;
        b = (double)currPoint.y - prevPoint.y;
        c = (double)currPoint.z - prevPoint.z;

        magnitude = (float)sqrt(a*a + b*b + c*c);

        if (delta + magnitude >= desiredOffset + 18) {
            float amountTraveled = desiredOffset + 18 - delta;
            float percentage = amountTraveled / magnitude;

            xPassTrans = currPoint.x - (currPoint.x - prevPoint.x) * percentage;
            yPassTrans = currPoint.y - (currPoint.y - prevPoint.y) * percentage;
            zPassTrans = currPoint.z - (currPoint.z - prevPoint.z) * percentage;

            tmpPoint.x = currPoint.x - (currPoint.x - prevPoint.x) * percentage;
            tmpPoint.y = currPoint.y - (currPoint.y - prevPoint.y) * percentage;
            tmpPoint.z = currPoint.z - (currPoint.z - prevPoint.z) * percentage;

            tmpPoint2.x = railTwoPoints[point].x - (railTwoPoints[point].x - railTwoPoints[nextIndex].x) * percentage;
            tmpPoint2.y = railTwoPoints[point].y - (railTwoPoints[point].y - railTwoPoints[nextIndex].y) * percentage;
            tmpPoint2.z = railTwoPoints[point].z - (railTwoPoints[point].z - railTwoPoints[nextIndex].z) * percentage;

            passengerAngle = -atan2(zPassTrans - prevPoint.z, xPassTrans - prevPoint.x) * 180 / PI;
            passengerZAngle = atan2(yPassTrans - prevPoint.y, abs(zPassTrans - prevPoint.z)) * 180 / PI;

            xPassTrans = (tmpPoint2.x + tmpPoint.x) / 2;
            yPassTrans = tmpPoint.y;
            zPassTrans = (tmpPoint.z + tmpPoint2.z) / 2;

            break;
        }
        else {
            delta += magnitude;
        }
        point--;
    }

    glPushMatrix();
    {
        glTranslatef(xPassTrans, yPassTrans + 4, zPassTrans);
        glRotated(passengerAngle, 0, 1, 0);
        glRotated(passengerZAngle, 0, 0, 1);
        drawCaboose(m_cabooseWidth, 3, 4, doingShadows);
    }
    glPopMatrix();
}
//Author Steven Volocyk
//draws the caboose using the rectangle function.
void TrainView::drawCaboose(float width, float height, float depth, bool doingShadows) {
    glBegin(GL_QUADS);
    {
        if (!doingShadows) {
            glColor3d(.7, .34, .34);
        }
        rectangle(width, height, depth);
    }
    glEnd();

    glPushMatrix();
    {
        glTranslatef(0, 2, 0);
        glBegin(GL_QUADS);
        {
            if (!doingShadows) {
                glColor3d(.7, .34, .34);
            }
            rectangle(width - 3, 3.0f, 4.0f);
        }
        glEnd();
    }
    glPopMatrix();

    glPushMatrix();
    {
        glTranslatef(-3.0f, -2.0f, 4.1f);
        drawWheel(doingShadows);
    }
    glPopMatrix();

    glPushMatrix();
    {
        glTranslatef(-3.0f, -2.0f, -4.1f);
        drawWheel(doingShadows);
    }
    glPopMatrix();

    glPushMatrix();
    {
        glTranslatef(3.0f, -2.0f, 4.1f);
        drawWheel(doingShadows);
    }
    glPopMatrix();

    glPushMatrix();
    {
        glTranslatef(3.0f, -2.0f, -4.1f);
        drawWheel(doingShadows);
    }
    glPopMatrix();
}
//Author Steven Volocyk
//draws passenger cars
void TrainView::drawPassengerCar(float width, float height, float depth, bool doingShadows) {
    glBegin(GL_QUADS);
    {
        if (!doingShadows) {
            glColor3d(.205, .201, .201);
        }
        rectangle(width, height, depth);
    }
    glEnd();

    glPushMatrix();
    {
        glTranslatef(7.0f, -2.0f, -4.1f);
        drawWheel(doingShadows);
    }
    glPopMatrix();

    glPushMatrix();
    {
        glTranslatef(7.0f, -2.0f, 4.1f);
        drawWheel(doingShadows);
    }
    glPopMatrix();

    //draw wheel1L
    glPushMatrix();
    {
        glTranslatef(-7.0f, -2.0f, 4.1f);
        drawWheel(doingShadows);
    }
    glPopMatrix();

    //draw wheel1R
    glPushMatrix();
    {
        glTranslatef(-7.0f, -2.0f, -4.1f);
        drawWheel(doingShadows);
    }
    glPopMatrix();
}
//Author Steven Volocyk
//draws one segment of the locomotive body
void TrainView::drawLocoBody(float width, float height, float depth, bool doingShadows) {

    glBegin(GL_QUADS);
    {
        if (!doingShadows) {
            glColor3d(0.0, 4.0, 1.0);
        }
        rectangle(width, height, depth);
    }
    glEnd();


    //draw locomotive cabin
    glPushMatrix();
    {
        glTranslatef(6.0f, 5.0f, 0.0f);
        drawLocoCabin(width - 12, height - 1, depth, doingShadows);
    }
    glPopMatrix();

    //draw wheel1L
    glPushMatrix();
    {
        glTranslatef(-7.0f, -2.0f, 4.1f);
        drawWheel(doingShadows);
    }
    glPopMatrix();

    //draw wheel1R
    glPushMatrix();
    {
        glTranslatef(-7.0f, -2.0f, -4.1f);
        drawWheel(doingShadows);
    }
    glPopMatrix();

    // w2L
    glPushMatrix();
    {
        glTranslatef(0.0f, -2.0f, 4.1f);
        drawWheel(doingShadows);
    }
    glPopMatrix();

    // w2R
    glPushMatrix();
    {
        glTranslatef(0.0f, -2.0f, -4.1f);
        drawWheel(doingShadows);
    }
    glPopMatrix();

    //w3L
    glPushMatrix();
    {
        glTranslatef(7.0f, -2.0f, 4.1f);
        drawWheel(doingShadows);
    }
    glPopMatrix();

    //w3R
    glPushMatrix();
    {
        glTranslatef(7.0f, -2.0f, -4.1f);
        drawWheel(doingShadows);
    }
    glPopMatrix();

    //draw smoke stack
    glPushMatrix();
    {
        glTranslatef(-6.0f, 7.0f, 0.0f);
        glRotated(90, 1.0, 0.0, 0.0);
        drawLocoStack(doingShadows);
    }
    glPopMatrix();
}
//Author Steven Volocyk
//draws the cabin of the body.
void TrainView::drawLocoCabin(float width, float height, float depth, bool doingShadows) {
    //glShadeModel(GL_FLAT);
    glBegin(GL_QUADS);
    {
        if (!doingShadows) {
            glColor3d(0, 4, 1);
        }
        rectangle(width, height, depth);
    }
    glEnd();
}
//Author Steven Volocyk
//draws wheels
void TrainView::drawWheel(bool doingShadows) {

    GLUquadricObj* quad;
    quad = gluNewQuadric();
    if (!doingShadows) {
        glColor3f(1.0, 1.0, 1.0);
    }
    gluDisk(quad, .1, 2, 30, 15);
}
//Author Steven Volocyk
//draws the locomotive smoke stack using cones.
void TrainView::drawLocoStack(bool doingShadows){
    GLUquadricObj* quad;
    quad = gluNewQuadric();

    if (!doingShadows) {
        glColor3f(1.0, 1.0, 1.0);
    }
    gluCylinder(quad, 2.5, 0, 8, 30, 15);
}
//Author Steven Volocyk
//draws the track using splinetype type of interpolation (linear or cardinal cubic). 
// Draws the train rails, places the train ties via utilization of arc length parameritization (series of small line segments)
void TrainView::drawTrack(TrainView* arg, bool doingShadows, int splineType, vector<Pnt3f>& interpolatedPoints, vector<Pnt3f>& offsetPoints) {
    size_t numControlPoints = world->points.size();

    //add size for line thickness.
    size_t numberOfSegments = 25; // number of lines approximating curve.
    float railOffset = 10; // width of track

    double tension = tw->tension->value();

    interpolatedPoints.resize(numberOfSegments * numControlPoints);
    offsetPoints.resize(numberOfSegments * numControlPoints + 1);

    glLineWidth(3.0);
    for (size_t i = 0; i < numControlPoints; i++) {

        Pnt3f point0 = world->points[i].pos;
        Pnt3f point1 = world->points[(i + 1) % numControlPoints].pos;
        Pnt3f point2 = world->points[(i + 2) % numControlPoints].pos;
        Pnt3f point3 = world->points[(i + 3) % numControlPoints].pos;

        for (size_t j = 0; j <= numberOfSegments; j++) {
            float position = (float)j / numberOfSegments;

            Pnt3f currentPoint;

            if (splineType == 1) {
                currentPoint.x = linearInterpolation(position, point0.x, point1.x);
                currentPoint.y = linearInterpolation(position, point0.y, point1.y);
                currentPoint.z = linearInterpolation(position, point0.z, point1.z);
            }
            if (splineType == 2) {
                currentPoint.x = cardinalCubic(position, (float)tension, point0.x, point1.x, point2.x, point3.x);
                currentPoint.y = cardinalCubic(position, (float)tension, point0.y, point1.y, point2.y, point3.y);
                currentPoint.z = cardinalCubic(position, (float)tension, point0.z, point1.z, point2.z, point3.z);
            }
            else {
                // todo, Bsplines.
            }

            if (j != numberOfSegments) {
                interpolatedPoints[i*numberOfSegments + j] = currentPoint;
            }
        }
    }

    //todo, calculate normal at each point, that is the offeset for each point of the second line.

    for (size_t k = 0; k < interpolatedPoints.size(); k++) {
        int nextPoint = k + 1;
        if (k + 1 == interpolatedPoints.size()) {
            nextPoint = 0;
        }
        Pnt3f tmpPoint, normalizedPoint;
        tmpPoint.x = (interpolatedPoints[nextPoint].x - interpolatedPoints[k].x);
        tmpPoint.y = (interpolatedPoints[nextPoint].y - interpolatedPoints[k].y);
        tmpPoint.z = (interpolatedPoints[nextPoint].z - interpolatedPoints[k].z);
        tmpPoint.normalize();

        // Rotate 90 degrees
        normalizedPoint.x = (float)(tmpPoint.x * cos(90.0) + tmpPoint.z * sin(90.0));
        normalizedPoint.y = tmpPoint.y;
        normalizedPoint.z = (float)(-tmpPoint.x * sin(90.0) + tmpPoint.z * cos(90.0));
        normalizedPoint = normalizedPoint * railOffset;
        offsetPoints[k].x = interpolatedPoints[k].x + normalizedPoint.x;
        offsetPoints[k].y = interpolatedPoints[k].y;
        offsetPoints[k].z = interpolatedPoints[k].z + normalizedPoint.z;
    }
    offsetPoints[offsetPoints.size() - 1] = offsetPoints[0];

    if (tw->trainCam->value()) {
        positionCameraInTrain(interpolatedPoints, offsetPoints);
    }
    if (!doingShadows) {
        glColor3f(0, 0, 0); // sets color to black.
    }
    glBegin(GL_LINE_STRIP);
    for (size_t i = 0; i < interpolatedPoints.size(); ++i)
    {
        Pnt3f& currentPoint = interpolatedPoints[i];
        glVertex3f(currentPoint.x, currentPoint.y, currentPoint.z); // handling last point of loop
    }
    glEnd();
    //draw second rail.
    glBegin(GL_LINE_STRIP);
    for (size_t y = 0; y < offsetPoints.size(); y++) {
        glVertex3f(offsetPoints[y].x, offsetPoints[y].y, offsetPoints[y].z);
    }
    glEnd();

    drawRailTies(interpolatedPoints, offsetPoints, railOffset, doingShadows);
}
//Author Steven Volocyk
//draws the rail ties.
void TrainView::drawRailTies(vector<Pnt3f>& railOnePoints, vector<Pnt3f>& railTwoPoints, float& railOffset, bool doingShadows) {
    int numRails = railOnePoints.size() % 5; // number of rails on the track
    int width = 0;
    int railCenter = 0;
    double angleOfRotation = 0.0;
    for (size_t i = 0; i < railOnePoints.size() - 1; i += 4) {
        Pnt3f tmpPoint, tmpPoint2;
        tmpPoint.x = railOnePoints[i].x;
        tmpPoint.y = railOnePoints[i].y;
        tmpPoint.z = railOnePoints[i].z;
        size_t nextIndex = i + 1;
        if (nextIndex >= railOnePoints.size())
        {
            nextIndex = 0;
        }
        tmpPoint2.x = railOnePoints[nextIndex].x;
        tmpPoint2.y = railOnePoints[nextIndex].y;
        tmpPoint2.z = railOnePoints[nextIndex].z;
        //drawRailTie(railOffset, tmpPoint.x, tmpPoint.y , tmpPoint.z);

        angleOfRotation = -atan2(tmpPoint2.z - tmpPoint.z, tmpPoint2.x - tmpPoint.x) * 180 / PI - 90;

        //angleOfRotation = atan  (tmpPoint2.x / tmpPoint2.z)  * 180 / PI;

        tmpPoint2.x = railTwoPoints[i].x;
        tmpPoint2.y = railTwoPoints[i].y;
        tmpPoint2.z = railTwoPoints[i].z;

        drawRailTie(railOffset + 5, (int)((tmpPoint2.x + tmpPoint.x) / 2), (int)tmpPoint.y,
            (int)(tmpPoint.z + tmpPoint2.z) / 2, angleOfRotation, doingShadows);
    }
}
//Author Steven Volocyk
//creates a single railTie.
void TrainView::drawRailTie(float offset, int x, int y, int z, double angle, bool doingShadows) {
    float width = offset;
    float height = 1.0;
    float depth = 1.0;
    const float BROWN[3] = { 0.35f, 0.16f, 0.14f };

    glPushMatrix();
    {
        glTranslatef((float)x, (float)y - 1.1f, (float)z);
        glRotated(angle, 0, 1, 0);
        glBegin(GL_QUADS);
        {
            if (!doingShadows) {
                glColor3fv(BROWN);
            }
            rectangle(width, height, depth);
        }
        glEnd();
    }
    glPopMatrix();
}
//Author Steven Volocyk
// general purpose rectangle creator, used in a lot of shape creation in the program.
void TrainView::rectangle(float width, float height, float length){

    glEnable(GL_NORMALIZE);
    width = width / 2;

    /* Back Surface */
    glVertex3f(width, height, -length);
    glVertex3f(width, -height, -length);
    glVertex3f(-width, -height, -length);
    glVertex3f(-width, height, -length);

    /* Front Surface */
    glVertex3f(width, height, length);
    glVertex3f(width, -height, length);
    glVertex3f(-width, -height, length);
    glVertex3f(-width, height, length);

    /* Top Surface */
    glVertex3f(width, height, -length);
    glVertex3f(-width, height, -length);
    glVertex3f(-width, height, length);
    glVertex3f(width, height, length);

    /* Bottom Surface */
    glVertex3f(width, -height, -length);
    glVertex3f(-width, -height, -length);
    glVertex3f(-width, -height, length);
    glVertex3f(width, -height, length);

    /* Left Surface */
    glVertex3f(-width, height, -length);
    glVertex3f(-width, -height, -length);
    glVertex3f(-width, -height, length);
    glVertex3f(-width, height, length);

    /* Right Surface */
    glVertex3f(width, height, -length);
    glVertex3f(width, -height, -length);
    glVertex3f(width, -height, length);
    glVertex3f(width, height, length);

    glDisable(GL_NORMALIZE);

}
//Author Steven Volocyk
//linear interpolation of control points
float TrainView::linearInterpolation(float u, float point0, float point1) {
    float a0, a1, total;

    a0 = point0;
    a1 = point1 - point0;
    total = a0 + u * a1;

    return total;
}
//Author Steven Volocyk
//cardinal cubic interpolation of control points.
float TrainView::cardinalCubic(float position, float tension, float point0, float point1, float point2, float point3) {

    float a0, a1, a2, a3;
    float calculatedTotal;

    a0 = point1;
    a1 = -tension * point0 + tension*point2;
    a2 = ((2 * tension) * point0) + ((tension - 3) * point1) + ((3 - (2 * tension)) * point2) + ((-tension * point3));
    a3 = (-tension * point0) + ((2 - tension) * point1) + ((tension - 2) * point2) + (tension * point3);

    calculatedTotal = a0 + a1*position + (a2*powf(position, 2)) + (a3*powf(position, 3));
    return calculatedTotal;

}
//Author Steven Volocyk
//draws the ground scenery, which consists of a handful of a different types of trees.
void TrainView::drawScenery(bool doingShadows) {

    //draws a bunch of trees. Normally I'd do this randomly in a loop,
    //but I didn't want to interfere with too many track designs so I kept it mostly to the corners and sides.
    glPushMatrix();
    {
        glTranslated(75, 0, 75);
        glRotated(-90, 1, 0, 0);
        if (!doingShadows) {
            glColor3f(.63f, .32f, .13f);
        }
        drawCylinder(1.0, .75, 7, 5, 10);

        glPushMatrix();
        {
            glTranslated(0, 0, 7);
            if (!doingShadows) {
                glColor3f(0.0f, .39f, 0.0f);
            }
            drawCylinder(4.0, 0, 6, 10, 10);
        }
        glPopMatrix();
    }
    glPopMatrix();

    glPushMatrix();
    {
        glTranslated(60, 0, 60);
        glRotated(-90, 1, 0, 0);
        if (!doingShadows) {
            glColor3f(.63f, .32f, .13f);
        }
        drawCylinder(1.0, .75, 7, 5, 10);

        glPushMatrix();
        {
            glTranslated(0, 0, 7);
            if (!doingShadows) {
                glColor3f(0.0f, .39f, 0.0f);
            }
            drawCylinder(4.0, 0, 6, 10, 10);
        }
        glPopMatrix();
    }
    glPopMatrix();

    glPushMatrix();
    {
        glTranslated(88, 0, 93);
        glRotated(-90, 1, 0, 0);
        if (!doingShadows) {
            glColor3f(.63f, .32f, .13f);
        }
        drawCylinder(1.0, .75, 7, 5, 10);

        glPushMatrix();
        {
            glTranslated(0, 0, 7);
            if (!doingShadows) {
                glColor3f(0.0f, .39f, 0.0f);
            }
            drawCylinder(2.0, 0, 12, 10, 10);
        }
        glPopMatrix();
    }
    glPopMatrix();

    glPushMatrix();
    {
        glTranslated(95, 0, 75);
        glRotated(-90, 1, 0, 0);
        if (!doingShadows) {
            glColor3f(.63f, .32f, .13f);
        }
        drawCylinder(1.0, .75, 7, 5, 10);

        glPushMatrix();
        {
            glTranslated(0, 0, 7);
            if (!doingShadows) {
                glColor3f(0.0f, .39f, 0.0f);
            }
            drawCylinder(4.0, 0, 6, 10, 10);
        }
        glPopMatrix();
    }
    glPopMatrix();

    glPushMatrix();
    {
        glTranslated(20, 0, 93);
        glRotated(-90, 1, 0, 0);
        if (!doingShadows) {
            glColor3f(.63f, .32f, .13f);
        }
        drawCylinder(1.0, .75, 7, 5, 10);

        glPushMatrix();
        {
            glTranslated(0, 0, 7);
            if (!doingShadows) {
                glColor3f(0.0f, .39f, 0.0f);
            }
            drawCylinder(2.0, 0, 14, 10, 10);
        }
        glPopMatrix();
    }
    glPopMatrix();

    glPushMatrix();
    {
        glTranslated(30, 0, 87);
        glRotated(-90, 1, 0, 0);
        if (!doingShadows) {
            glColor3f(.93f, .32f, .13f);
        }
        drawCylinder(1.0, .75, 7, 5, 10);

        glPushMatrix();
        {
            glTranslated(0, 0, 7);
            if (!doingShadows) {
                glColor3f(0.0f, .39f, 0.0f);
            }
            drawCylinder(3.0, 0, 14, 10, 10);
        }
        glPopMatrix();
    }
    glPopMatrix();

    glPushMatrix();
    {
        glTranslated(0, 0, 93);
        glRotated(-90, 1, 0, 0);
        if (!doingShadows) {
            glColor3f(.63f, .32f, .13f);
        }
        drawCylinder(1.0, .75, 7, 5, 10);

        glPushMatrix();
        {
            glTranslated(0, 0, 7);
            if (!doingShadows) {
                glColor3f(0.0f, .39f, 0.0f);
            }
            drawCylinder(2.0, 0, 12, 10, 10);
        }
        glPopMatrix();
    }
    glPopMatrix();

    glPushMatrix();
    {
        glTranslated(-88, 0, 93);
        glRotated(-90, 1, 0, 0);
        if (!doingShadows) {
            glColor3f(.63f, .32f, .13f);
        }
        drawCylinder(1.0, .75, 7, 5, 10);

        glPushMatrix();
        {
            glTranslated(0, 0, 7);
            if (!doingShadows) {
                glColor3f(0.0f, .39f, 0.0f);
            }
            drawCylinder(2.0, 0, 12, 10, 10);
        }
        glPopMatrix();
    }
    glPopMatrix();

    glPushMatrix();
    {
        glTranslated(-75, 0, 75);
        glRotated(-90, 1, 0, 0);
        if (!doingShadows) {
            glColor3f(.63f, .32f, .13f);
        }
        drawCylinder(1.0, .75, 7, 5, 10);

        glPushMatrix();
        {
            glTranslated(0, 0, 7);
            if (!doingShadows) {
                glColor3f(0.0f, .39f, 0.0f);
            }
            drawCylinder(4.0, 0, 6, 10, 10);
        }
        glPopMatrix();
    }
    glPopMatrix();

    glPushMatrix();
    {
        glTranslated(-55, 0, 75);
        glRotated(-90, 1, 0, 0);
        if (!doingShadows) {
            glColor3f(.63f, .32f, .13f);
        }
        drawCylinder(1.0, .75, 7, 5, 10);

        glPushMatrix();
        {
            glTranslated(0, 0, 7);
            if (!doingShadows) {
                glColor3f(0.0f, .39f, 0.0f);
            }
            drawCylinder(4.0, 0, 16, 10, 10);
        }
        glPopMatrix();
    }
    glPopMatrix();

    glPushMatrix();
    {
        glTranslated(-75, 0, -75);
        glRotated(-90, 1, 0, 0);
        if (!doingShadows) {
            glColor3f(.63f, .32f, .13f);
        }
        drawCylinder(1.0, .75, 7, 5, 10);

        glPushMatrix();
        {
            glTranslated(0, 0, 7);
            if (!doingShadows) {
                glColor3f(0.0f, .39f, 0.0f);
            }
            drawCylinder(4.0, 0, 16, 10, 10);
        }
        glPopMatrix();
    }
    glPopMatrix();

    glPushMatrix();
    {
        glTranslated(-95, 0, -85);
        glRotated(-90, 1, 0, 0);
        if (!doingShadows) {
            glColor3f(.63f, .32f, .13f);
        }
        drawCylinder(1.0, .75, 7, 5, 10);

        glPushMatrix();
        {
            glTranslated(0, 0, 7);
            if (!doingShadows) {
                glColor3f(0.0f, .39f, 0.0f);
            }
            drawCylinder(4.0, 0, 16, 10, 10);
        }
        glPopMatrix();
    }
    glPopMatrix();

    glPushMatrix();
    {
        glTranslated(-35, 0, -75);
        glRotated(-90, 1, 0, 0);
        if (!doingShadows) {
            glColor3f(.63f, .32f, .13f);
        }
        drawCylinder(1.0, .75, 7, 5, 10);

        glPushMatrix();
        {
            glTranslated(0, 0, 7);
            if (!doingShadows) {
                glColor3f(0.0f, .39f, 0.0f);
            }
            drawCylinder(4.0, 0, 6, 10, 10);
        }
        glPopMatrix();
    }
    glPopMatrix();

    glPushMatrix();
    {
        glTranslated(-75, 0, -25);
        glRotated(-90, 1, 0, 0);
        if (!doingShadows) {
            glColor3f(.63f, .32f, .13f);
        }
        drawCylinder(1.0, .75, 7, 5, 10);

        glPushMatrix();
        {
            glTranslated(0, 0, 7);
            if (!doingShadows) {
                glColor3f(0.0f, .39f, 0.0f);
            }
            drawCylinder(4.0, 0, 6, 10, 10);
        }
        glPopMatrix();
    }
    glPopMatrix();

    glPushMatrix();
    {
        glTranslated(75, 0, -25);
        glRotated(-90, 1, 0, 0);
        if (!doingShadows) {
            glColor3f(.63f, .32f, .13f);
        }
        drawCylinder(1.0, .75, 7, 5, 10);

        glPushMatrix();
        {
            glTranslated(0, 0, 7);
            if (!doingShadows) {
                glColor3f(0.0f, .39f, 0.0f);
            }
            drawCylinder(5.0, 0, 16, 10, 10);
        }
        glPopMatrix();
    }
    glPopMatrix();

    glPushMatrix();
    {
        glTranslated(75, 0, -65);
        glRotated(-90, 1, 0, 0);
        if (!doingShadows) {
            glColor3f(.63f, .32f, .13f);
        }
        drawCylinder(1.0, .75, 7, 5, 10);

        glPushMatrix();
        {
            glTranslated(0, 0, 7);
            if (!doingShadows) {
                glColor3f(0.0f, .39f, 0.0f);
            }
            drawCylinder(3.0, 0, 18, 10, 10);
        }
        glPopMatrix();
    }
    glPopMatrix();

    glPushMatrix();
    {
        glTranslated(85, 0, -85);
        glRotated(-90, 1, 0, 0);
        if (!doingShadows) {
            glColor3f(.63f, .32f, .13f);
        }
        drawCylinder(1.0, .75, 4, 5, 10);

        glPushMatrix();
        {
            glTranslated(0, 0, 5);
            if (!doingShadows) {
                glColor3f(0.0f, .39f, 0.0f);
            }
            drawCylinder(2.0, 0, 6, 10, 10);
        }
        glPopMatrix();
    }
    glPopMatrix();

    glPushMatrix();
    {
        glTranslated(45, 0, -85);
        glRotated(-90, 1, 0, 0);
        if (!doingShadows) {
            glColor3f(.63f, .32f, .13f);
        }
        drawCylinder(1.0, .75, 7, 5, 10);

        glPushMatrix();
        {
            glTranslated(0, 0, 7);
            if (!doingShadows) {
                glColor3f(0.0f, .39f, 0.0f);
            }
            drawCylinder(3.0, 0, 18, 10, 10);
        }
        glPopMatrix();
    }
    glPopMatrix();
}
//Author Steven Volocyk
//used for general purpose cylinders or cones.
void TrainView::drawCylinder(double base, double top, double height, int slices, int stacks) {

    GLUquadricObj* quad;
    quad = gluNewQuadric();
    //gluQuadricDrawStyle(cyl, GLU_LINE);
    gluCylinder(quad, base, top, height, slices, stacks);
}

// this tries to see which control point is under the mouse
// (for when the mouse is clicked)
// it uses OpenGL picking - which is always a trick
// TODO: if you want to pick things other than control points, or you
// changed how control points are drawn, you might need to change this
void TrainView::doPick()
{
    make_current();		// since we'll need to do some GL stuff

    int mx = Fl::event_x(); // where is the mouse?
    int my = Fl::event_y();

    // get the viewport - most reliable way to turn mouse coords into GL coords
    int viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    // set up the pick matrix on the stack - remember, FlTk is
    // upside down!
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPickMatrix((double)mx, (double)(viewport[3] - my), 5, 5, viewport);

    // now set up the projection
    setProjection();

    // now draw the objects - but really only see what we hit
    GLuint buf[100];
    glSelectBuffer(100, buf);
    glRenderMode(GL_SELECT);
    glInitNames();
    glPushName(0);

    // draw the cubes, loading the names as we go
    for (size_t i = 0; i<world->points.size(); ++i) {
        glLoadName((GLuint)(i + 1));
        world->points[i].draw();
    }

    // go back to drawing mode, and see how picking did
    int hits = glRenderMode(GL_RENDER);
    if (hits) {
        // warning; this just grabs the first object hit - if there
        // are multiple objects, you really want to pick the closest
        // one - see the OpenGL manual 
        // remember: we load names that are one more than the index
        selectedCube = buf[3] - 1;
    }
    else // nothing hit, nothing selected
        selectedCube = -1;

    printf("Selected Cube %d\n", selectedCube);
}

// CVS Header - if you don't know what this is, don't worry about it
// This code tells us where the original came from in CVS
// Its a good idea to leave it as-is so we know what version of
// things you started with
// $Header: /p/course/cs559-gleicher/private/CVS/TrainFiles/TrainView.cpp,v 1.10 2009/11/08 21:34:13 gleicher Exp $