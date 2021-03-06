//-
// ==========================================================================
// Copyright (C) 1995 - 2006 Autodesk, Inc. and/or its licensors.  All 
// rights reserved.
//
// The coded instructions, statements, computer programs, and/or related 
// material (collectively the "Data") in these files contain unpublished 
// information proprietary to Autodesk, Inc. ("Autodesk") and/or its 
// licensors, which is protected by U.S. and Canadian federal copyright 
// law and by international treaties.
//
// The Data is provided for use exclusively by You. You have the right 
// to use, modify, and incorporate this Data into other products for 
// purposes authorized by the Autodesk software license agreement, 
// without fee.
//
// The copyright notices in the Software and this entire statement, 
// including the above license grant, this restriction and the 
// following disclaimer, must be included in all copies of the 
// Software, in whole or in part, and all derivative works of 
// the Software, unless such copies or derivative works are solely 
// in the form of machine-executable object code generated by a 
// source language processor.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND. 
// AUTODESK DOES NOT MAKE AND HEREBY DISCLAIMS ANY EXPRESS OR IMPLIED 
// WARRANTIES INCLUDING, BUT NOT LIMITED TO, THE WARRANTIES OF 
// NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR 
// PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE, OR 
// TRADE PRACTICE. IN NO EVENT WILL AUTODESK AND/OR ITS LICENSORS 
// BE LIABLE FOR ANY LOST REVENUES, DATA, OR PROFITS, OR SPECIAL, 
// DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES, EVEN IF AUTODESK 
// AND/OR ITS LICENSORS HAS BEEN ADVISED OF THE POSSIBILITY 
// OR PROBABILITY OF SUCH DAMAGES.
//
// ==========================================================================
//+

#include <math.h>

#include <maya/MPxNode.h>
#include <maya/MIOStream.h>
#include <maya/MString.h>
#include <maya/MTypeId.h>
#include <maya/MPlug.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFloatVector.h>
#include <maya/MPointArray.h>
#include <maya/MFnPlugin.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MFnMesh.h>
#include <maya/MColor.h>
#include <maya/MItMeshVertex.h>

class cvColorShader : public MPxNode
{
	public:
                    cvColorShader();
    virtual         ~cvColorShader();

    virtual MStatus compute( const MPlug&, MDataBlock& );
	virtual void    postConstructor();

    static  void *  creator();
    static  MStatus initialize();

	//  Id tag for use with binary file format
    static  MTypeId id;

	private:

	static inline float dotProd(const MFloatVector &, const MFloatVector &); 

	// Input attributes

	static MObject aReverseAlpha;

	static MObject aPointObj;				// Implicit attribute
	static MObject aPrimitiveId;			// Implicit attribute

	// Output attributes
	static MObject aOutColor;
	static MObject aOutAlpha;
};

// Static data
MTypeId cvColorShader::id( 0x8000f );

// Attributes 
MObject cvColorShader::aReverseAlpha;
MObject cvColorShader::aPointObj;
MObject cvColorShader::aPrimitiveId;
MObject cvColorShader::aOutColor;
MObject cvColorShader::aOutAlpha;

void cvColorShader::postConstructor( )
{
	setMPSafe(true);
}

cvColorShader::cvColorShader()
{
}

cvColorShader::~cvColorShader()
{
}

void * cvColorShader::creator()
{
    return new cvColorShader();
}

MStatus cvColorShader::initialize()
{
    MFnNumericAttribute nAttr;

	aReverseAlpha = nAttr.create( "reverseAlpha", "ra", 
								  MFnNumericData::kBoolean);
	CHECK_MSTATUS ( nAttr.setDefault( true ) );

    aPointObj  = nAttr.createPoint( "pointObj", "po" );
	CHECK_MSTATUS ( nAttr.setStorable(false) );
    CHECK_MSTATUS ( nAttr.setHidden(true) );

	aPrimitiveId = nAttr.create( "primitiveId", "pi", MFnNumericData::kLong);
    CHECK_MSTATUS ( nAttr.setHidden(true) );

    aOutColor = nAttr.createColor( "outColor", "oc" );
    CHECK_MSTATUS ( nAttr.setStorable(false) );
    CHECK_MSTATUS ( nAttr.setReadable(true) );
    CHECK_MSTATUS ( nAttr.setWritable(false) );

	aOutAlpha = nAttr.create( "outAlpha", "oa", MFnNumericData::kFloat);
	CHECK_MSTATUS (  nAttr.setDisconnectBehavior(MFnAttribute::kReset) );
    CHECK_MSTATUS ( nAttr.setStorable(false) );
    CHECK_MSTATUS ( nAttr.setReadable(true) );
    CHECK_MSTATUS ( nAttr.setWritable(false) );

    CHECK_MSTATUS ( addAttribute(aPointObj) );
    CHECK_MSTATUS ( addAttribute(aOutColor) );
    CHECK_MSTATUS ( addAttribute(aOutAlpha) );
    CHECK_MSTATUS ( addAttribute(aReverseAlpha) );
    CHECK_MSTATUS ( addAttribute(aPrimitiveId) );

    CHECK_MSTATUS ( attributeAffects(aPointObj,     aOutColor) );
    CHECK_MSTATUS ( attributeAffects(aPrimitiveId,  aOutColor) );

    CHECK_MSTATUS ( attributeAffects(aReverseAlpha, aOutAlpha) );
    CHECK_MSTATUS ( attributeAffects(aPointObj,     aOutAlpha) );
    CHECK_MSTATUS ( attributeAffects(aPrimitiveId,  aOutAlpha) );

	return MS::kSuccess;
}

// dot product on vectors
inline float cvColorShader::dotProd(
	const MFloatVector & v1,
	const MFloatVector & v2) 
{
	return  v1.x*v2.x +  v1.y*v2.y + v1.z*v2.z;
}

//
//
///////////////////////////////////////////////////////////////////////////////
MStatus cvColorShader::compute( const MPlug& plug, MDataBlock& block ) 
{
	if ((plug != aOutColor) && (plug.parent() != aOutColor) && 
		(plug != aOutAlpha))
		return MS::kUnknownParameter;

	MStatus status;
	MObject thisNode = thisMObject();

	bool rev_flag = block.inputValue(aReverseAlpha).asBool();
	long triangleId = block.inputValue(aPrimitiveId).asLong();

	// Location of the point we are shading
	MFloatVector& pointObj = block.inputValue( aPointObj ).asFloatVector();

	// Find the Mesh object
	MPlug outColorPlug = MFnDependencyNode(thisNode).findPlug("outColor", 
															  &status);
	if( MStatus::kSuccess != status )
		return MS::kFailure;
		
	MItDependencyGraph depIt( outColorPlug, MFn::kShadingEngine,
							  MItDependencyGraph::kDownstream, 
							  MItDependencyGraph::kBreadthFirst,
							  MItDependencyGraph::kNodeLevel, 
							  &status);
	if( MStatus::kSuccess != status )
		return MS::kFailure;
	
	CHECK_MSTATUS ( depIt.enablePruningOnFilter() );

	MObject shadingEngineNode = depIt.thisNode(); // test
	
	MPlug dagSetMembersPlug = MFnDependencyNode(shadingEngineNode).findPlug(
		"dagSetMembers", &status );
	if( MStatus::kSuccess != status )
		return MS::kFailure;
	MPlug dagSetMembersElementPlug = dagSetMembersPlug.elementByLogicalIndex( 
		0, &status );
	if( MStatus::kSuccess != status )
		return MS::kFailure;
	
	MPlugArray meshPlugArray;
	dagSetMembersElementPlug.connectedTo( meshPlugArray, true, false, &status);
	if( MStatus::kSuccess != status )
		return MS::kFailure;
	
	MObject meshNode = meshPlugArray[0].node();

	// Find the face a local triangle number.
	int polygonId = 0;
	int preIndex = 0;

	int i;
	MFnMesh meshFn( meshNode );
	MItMeshPolygon fIter( meshNode );
	int numPolygons = meshFn.numPolygons();		// Number of faces
	for( i = 0; i < numPolygons; i++ )
	{
		int nTri;
		fIter.setIndex( i, preIndex );
		fIter.numTriangles(nTri);

		if( triangleId < nTri )
		{
			polygonId = i;
			break;
		}
		triangleId -= nTri;
	}

	MPointArray posArray;
	MIntArray vIndexArray;

	// We are shading the triangleId th triangle in face polygonId
	status = fIter.getTriangle(triangleId, posArray, vIndexArray,
							   MSpace::kObject);
	if( MStatus::kSuccess != status )
		return MS::kFailure;

	MFloatVector pos1;
	MFloatVector pos2;
	MFloatVector pos3;
	pos1.x = (float)posArray[0].x;
	pos1.y = (float)posArray[0].y;
	pos1.z = (float)posArray[0].z;
	pos2.x = (float)posArray[1].x;
	pos2.y = (float)posArray[1].y;
	pos2.z = (float)posArray[1].z;
	pos3.x = (float)posArray[2].x;
	pos3.y = (float)posArray[2].y;
	pos3.z = (float)posArray[2].z;

	MColor color1;
	MColor color2;
	MColor color3;

	MItMeshVertex vIter( meshNode );
	CHECK_MSTATUS ( vIter.setIndex( vIndexArray[0], preIndex) );
	CHECK_MSTATUS ( vIter.getColor( color1, polygonId ) );
	
	CHECK_MSTATUS ( vIter.setIndex( vIndexArray[1], preIndex) );
	CHECK_MSTATUS ( vIter.getColor( color2, polygonId ) );

	CHECK_MSTATUS ( vIter.setIndex( vIndexArray[2], preIndex) );
	CHECK_MSTATUS ( vIter.getColor( color3, polygonId ) );

	// Compute the barycentric coordinates of the sample.

	pointObj = pointObj - pos3;				// Translate pos3 to origin
    pos1 = pos1 - pos3;
    pos2 = pos2 - pos3;

    MFloatVector norm = pos1 ^ pos2;		// Triangle normal
    float len = dotProd(norm, norm);
    len = dotProd(norm, pointObj)/len;

    pointObj = pointObj - (len * norm);		// Make sure the point is
											// in the triangle

    float aa = dotProd(pos1, pos1);
    float bb = dotProd(pos2, pos2);
    float ab = dotProd(pos1, pos2);
    float am = dotProd(pos1, pointObj);
    float bm = dotProd(pos2, pointObj);
    float det = aa*bb - ab*ab;

    // a, b, c are the barycentric coordinates (assuming pnt
    // is in the triangle plane, best least square fit
    // otherwise.
    //
    float a = (am*bb - bm*ab) / det;
    float b = (bm*aa - am*ab) / det;
	float c = 1-a-b;

	MColor resultColor = (a*color1) + (b*color2) + (c*color3);

	if( rev_flag == true )
		resultColor.a = 1.0f - resultColor.a;

	MDataHandle outColorHandle = block.outputValue( aOutColor );
	MFloatVector& outColor = outColorHandle.asFloatVector();
	outColor.x = resultColor.r;
	outColor.y = resultColor.g;
	outColor.z = resultColor.b;
	outColorHandle.setClean();

	MDataHandle outAlphaHandle = block.outputValue( aOutAlpha );
	float& outAlpha = outAlphaHandle.asFloat();
	outAlpha = resultColor.a;
	outAlphaHandle.setClean();

	return MS::kSuccess;
}


/////////////////////////////////////////////////////////////////////////////////////////
MStatus initializePlugin( MObject obj )
{ 
	const MString UserClassify( "utility/color" );
	
	MFnPlugin plugin( obj, PLUGIN_COMPANY, "4.5", "Any");
	CHECK_MSTATUS ( plugin.registerNode( "cvColorShader", cvColorShader::id, 
						 cvColorShader::creator, 
						 cvColorShader::initialize,
						 MPxNode::kDependNode, &UserClassify ) );

	return MS::kSuccess;
}

MStatus uninitializePlugin( MObject obj )
{
	MFnPlugin plugin( obj );
	CHECK_MSTATUS ( plugin.deregisterNode( cvColorShader::id ) );

	return MS::kSuccess;
}
