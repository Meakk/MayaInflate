#include <iostream>

#include <maya/MIOStream.h>
#include <maya/MSyntax.h>
#include <maya/MArgParser.h>
#include <maya/MArgList.h>
#include <maya/MGlobal.h>
#include <maya/MFnPlugin.h>
#include <maya/MPxCommand.h>
#include <maya/MFnMesh.h>
#include <maya/MSelectionList.h>
#include <maya/MDagPath.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFloatArray.h>

#define iterFlag       "-i"
#define iterFlagLong   "-iterations"
#define ratioFlag      "-r"
#define ratioFlagLong  "-ratio"


class inflate : public MPxCommand
{
public:
  MStatus doIt(const MArgList& args);
  bool isUndoable() const { return true; }
  MStatus undoIt();
  MStatus redoIt();
  static void* creator();
  static MSyntax newSyntax();
  MStatus parseArgs(const MArgList &args);
  MStatus applyInflation(MFnMesh &mesh);

private:
  double _ratio;
  unsigned int _iterations;
};

MSyntax inflate::newSyntax()
{
  MSyntax syntax;
  syntax.addFlag(iterFlag, iterFlagLong, MSyntax::kUnsigned);
  syntax.addFlag(ratioFlag, ratioFlagLong, MSyntax::kDouble);
  return syntax;
}

MStatus inflate::parseArgs(const MArgList &args)
{
  MArgParser argData(syntax(), args);

  if (argData.isFlagSet(iterFlag))
  {
    if (argData.getFlagArgument(iterFlag, 0, _iterations) != MS::kSuccess)
    {
      return MS::kFailure;
    }
  }
  if (argData.isFlagSet(ratioFlag))
  {
    if (argData.getFlagArgument(ratioFlag, 0, _ratio) != MS::kSuccess)
    {
      return MS::kFailure;
    }
  }

  return MS::kSuccess;
}

MStatus inflate::undoIt()
{
  return MS::kSuccess;
}

MStatus inflate::redoIt()
{
  return MS::kSuccess;
}

MStatus inflate::doIt(const MArgList& args) {
  cout << "args :" << endl;

  for (unsigned int i = 0; i < args.length(); i++)
  {
    cout << "\t" << args.asString(i).asChar() << endl;
  }

  if (parseArgs(args) != MS::kSuccess)
  {
    cout << "Error parsing arguments" << endl;
    return MS::kFailure;
  }

  // get a list of the currently selected items
  MSelectionList selected;
  MGlobal::getActiveSelectionList(selected);

  // iterate through the list of items returned
  for (unsigned int i = 0; i<selected.length(); ++i)
  {
    // returns the i'th selected dependency node
    //MObject obj;
    //selected.getDependNode(i, obj);

    MDagPath meshDagPath;
    selected.getDagPath(i, meshDagPath);

    meshDagPath.extendToShape();

    // check if it's a mesh
    if (meshDagPath.node().hasFn(MFn::kMesh))
    {
      applyInflation((MFnMesh)meshDagPath.node());
    }
  }

  return MS::kSuccess;
}

unsigned int reduceInt(MIntArray& iArray)
{
  unsigned int result = 0;
  for (unsigned int i = 0; i < iArray.length(); i++)
  {
    result += iArray[i];
  }
  return result;
}

float computeVolume(unsigned int nbTriangles, MIntArray& triangleVertices, MFloatPointArray& vertices)
{
  float volume = 0.f;

  for (unsigned int i = 0; i < nbTriangles; i++)
  {
    unsigned int triangle[] = { triangleVertices[3 * i], triangleVertices[3 * i + 1], triangleVertices[3 * i + 2] };

    volume += (MFloatVector(vertices[triangle[0]]) ^ MFloatVector(vertices[triangle[1]])) * MFloatVector(vertices[triangle[2]]);
  }

  return volume;
}

void computeVolumeGradient(unsigned int nbTriangles, MIntArray& triangleVertices, MFloatPointArray& vertices, MFloatVectorArray& gradient)
{
  gradient = MFloatVectorArray(gradient.length());

  for (unsigned int i = 0; i < nbTriangles; i++)
  {
    unsigned int triangle[] = { triangleVertices[3 * i], triangleVertices[3 * i + 1], triangleVertices[3 * i + 2] };
    MFloatVector p0(vertices[triangle[0]]);
    MFloatVector p1(vertices[triangle[1]]);
    MFloatVector p2(vertices[triangle[2]]);

    gradient[triangle[0]] += p1 ^ p2;
    gradient[triangle[1]] += p2 ^ p0;
    gradient[triangle[2]] += p0 ^ p1;
  }
}

float normSquare(MFloatVectorArray & gradient)
{
  float result = 0.f;

  for (unsigned int i = 0; i < gradient.length(); i++)
  {
    const MFloatVector& currentVector = gradient[i];
    result += currentVector * currentVector;
  }

  return result;
}

MStatus inflate::applyInflation(MFnMesh &mesh)
{
  int nbVertices = mesh.numVertices();

  MIntArray triangles;
  MIntArray triangleVertices;
  MFloatPointArray vertices;

  mesh.getPoints(vertices);
  mesh.getTriangles(triangles, triangleVertices);

  unsigned int nbTriangles = reduceInt(triangles);
  unsigned int nbEdges = mesh.numEdges();

  float initialVolume = computeVolume(nbTriangles, triangleVertices, vertices);
  MFloatArray initialEdges(mesh.numEdges());

  for (unsigned int i = 0; i < nbEdges; i++)
  {
    int edge[2];
    mesh.getEdgeVertices(i, edge);
    initialEdges[i] = vertices[edge[0]].distanceTo(vertices[edge[1]]);
  }

  MFloatVectorArray gradient(vertices.length());

  for (unsigned int i = 0; i < _iterations; i++)
  {
    //solve edges, Gauss-Seidel fashion
    for (unsigned int j = 0; j < nbEdges; j++)
    {
      int edge[2];
      mesh.getEdgeVertices(j, edge);
      MFloatVector direction = vertices[edge[0]] - vertices[edge[1]];
      float norm = direction.length();
      MFloatVector displacement = ((norm - initialEdges[j]) / norm)*direction;
      vertices[edge[0]] -= displacement;
      vertices[edge[1]] += displacement;
    }

    //solve volume
    computeVolumeGradient(nbTriangles, triangleVertices, vertices, gradient);

    float scalingFactor = (computeVolume(nbTriangles, triangleVertices, vertices) - _ratio*initialVolume) / normSquare(gradient);

    for (unsigned int j = 0; j < vertices.length(); j++)
    {
      vertices[j] -= scalingFactor*gradient[j];
    }
  }

  mesh.setPoints(vertices);

  cout << "apply inflation on " << mesh.fullPathName() << " ( " << nbVertices << " vertices.)" << endl;
  cout << "initial volume : " << initialVolume << endl;

  return MS::kSuccess;
}

void* inflate::creator() {
  return new inflate;
}

MStatus initializePlugin(MObject obj) {
  MFnPlugin plugin(obj, "Michael MIGLIORE", "0.1", "Any");
  plugin.registerCommand("inflate", inflate::creator, inflate::newSyntax);
  return MS::kSuccess;
}

MStatus uninitializePlugin(MObject obj) {
  MFnPlugin plugin(obj);
  plugin.deregisterCommand("inflate");
  return MS::kSuccess;
}



/*
  SHELF

  global string $ratioField;
  global string $iterField;

  global proc goCommand() {
  global string $valueRatio;
  global string $valueIter;
  global string $ratioField;
  global string $iterField;

  // query the value from the float field
  $valueRatio = `floatField -query -value $ratioField`;
  $valueIter = `intField -query -value $iterField`;
  }

  {
  string $window = `window -widthHeight 160 60 -sizeable false -title "Inflate" -maximizeButton false -minimizeButton false`;
  gridLayout -numberOfColumns 2 -cellWidthHeight 80 20;
  text "volume ratio";
  $ratioField = `floatField -precision 2 -minValue 0 -value 1 -step .01`;
  text "iterations";
  $iterField = `intField -minValue 1 -value 3 -maxValue 100`;
  button -label "OK" -command goCommand;
  showWindow $window;
  }

  */
