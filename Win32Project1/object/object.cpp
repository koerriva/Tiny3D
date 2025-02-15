#include "object.h"
#include <stdlib.h>
#include "../node/node.h"
#include "../constants/constants.h"

Object::Object() {
	parent = NULL;
	position = vec3(0.0);
	size = vec3(1.0);
	localTransformMatrix.LoadIdentity();
	normalMatrix.LoadIdentity();

	translateMat.LoadIdentity();
	rotateMat.LoadIdentity();
	scaleMat.LoadIdentity();

	mesh = NULL;
	meshMid = NULL;
	meshLow = NULL;
	bounding = NULL;
	material = -1;
	boundCenter = vec3(0.0);
	localBoundPosition = vec3(0.0);

	billboard = NULL;
	genShadow = true;
	detailLevel = 2;

	transforms = NULL;
	transformsFull = NULL;
	rotateQuat = MatrixToQuat(rotateMat);
	boundInfo = vec4(0.0);

	shapeOffset = vec3(0.0);
	collisionShape = NULL;
	collisionObject = NULL;
	mass = 0;
	dynamic = false;
	hasPhysic = true;

	sounds.clear();
}

Object::Object(const Object& rhs) {
	parent = rhs.parent;
	if (rhs.billboard)
		billboard = new Billboard(rhs.billboard->data[0], rhs.billboard->data[1], rhs.billboard->material);
	else
		billboard = NULL;
	transforms = NULL;
	transformsFull = NULL;
	rotateQuat = rhs.rotateQuat;
	boundInfo = rhs.boundInfo;
	material = rhs.material;

	shapeOffset = rhs.shapeOffset;
	collisionShape = NULL;
	collisionObject = NULL;
	mass = rhs.mass;
	dynamic = rhs.dynamic;
	hasPhysic = rhs.hasPhysic;
	boundCenter = rhs.boundCenter;
	localBoundPosition = rhs.localBoundPosition;

	sounds.clear();
	std::map<std::string, SoundObject*>::iterator it;
	std::map<std::string, SoundObject*> soundsToCopy = rhs.sounds;
	for (it = soundsToCopy.begin(); it != soundsToCopy.end(); ++it)
		sounds[it->first] = new SoundObject(*it->second);
}

Object::~Object() {
	if (bounding) delete bounding; bounding = NULL;
	if (billboard) delete billboard; billboard = NULL;

	if (transforms) free(transforms); transforms = NULL;
	if (transformsFull) free(transformsFull); transformsFull = NULL;
	
	if (collisionShape) delete collisionShape;

	std::map<std::string, SoundObject*>::iterator it;
	for (it = sounds.begin(); it != sounds.end(); ++it)
		delete it->second;
	sounds.clear();
}

void Object::initMatricesData() {
	transforms = (float*)malloc(4 * sizeof(float));
	transformsFull = (buff*)malloc(16 * sizeof(buff));

	transforms[0] = 0.0, transforms[1] = 0.0, transforms[2] = 0.0, transforms[3] = 1.0;
	transformsFull[0] = transforms[0];
	transformsFull[1] = transforms[1];
	transformsFull[2] = transforms[2];
	transformsFull[3] = transforms[3];
	transformsFull[4] = rotateQuat.x;
	transformsFull[5] = rotateQuat.y;
	transformsFull[6] = rotateQuat.z;
	transformsFull[7] = rotateQuat.w;
}

void Object::caculateLocalAABB(bool looseWidth, bool looseAll) {
	if (!mesh) return; // caculate AABB by yourself
	int vertexCount = mesh->vertexCount;
	if (vertexCount <= 0) return;
	vec4* vertices = mesh->vertices;
	mat4 localRotateMatrix = GetRotateAndScale(localTransformMatrix);
	vec4 first4 = localRotateMatrix * vertices[0];
	float firstInvw = 1.0 / first4.w;
	float sx = first4.x * firstInvw;
	float sy = first4.y * firstInvw;
	float sz = first4.z * firstInvw;
	float lx = sx;
	float ly = sy;
	float lz = sz;
	for (int i = 1; i < vertexCount; i++) {
		vec4 vertex4 = vertices[i];
		vec4 local4 = localRotateMatrix * vertex4;
		float invw = 1.0 / local4.w;
		vec3 local3(local4.x * invw, local4.y * invw, local4.z * invw);
		sx = sx > local3.x ? local3.x : sx;
		sy = sy > local3.y ? local3.y : sy;
		sz = sz > local3.z ? local3.z : sz;
		lx = lx < local3.x ? local3.x : lx;
		ly = ly < local3.y ? local3.y : ly;
		lz = lz < local3.z ? local3.z : lz;
	}
	vec3 minVertex(sx, sy, sz), maxVertex(lx, ly, lz);
	if (!bounding) bounding = new AABB(minVertex, maxVertex);
	else ((AABB*)bounding)->update(minVertex, maxVertex);

	if (looseWidth) {
		AABB* aabb = (AABB*)bounding;
		float maxWidth = aabb->sizex > aabb->sizez ? aabb->sizex : aabb->sizez;
		aabb->update(maxWidth, aabb->sizey, maxWidth);
	} 
	
	if (looseAll) {
		AABB* aabb = (AABB*)bounding;
		float maxSide = aabb->sizex;
		maxSide = maxSide < aabb->sizey ? aabb->sizey : maxSide;
		maxSide = maxSide < aabb->sizez ? aabb->sizez : maxSide;
		aabb->update(maxSide, maxSide, maxSide);
	}
	boundCenter = bounding->position;
	localBoundPosition = boundCenter + GetTranslate(localTransformMatrix);
	bounding->update(localBoundPosition);
}

void Object::caculateCollisionShape() {
	if (collisionShape) delete collisionShape;
	if (!mesh) { // Animation object use node bounding box
		vec3 halfSize = ((AABB*)parent->boundingBox)->halfSize;
		collisionShape = new CollisionShape(halfSize);
	} else {
		float sx = MAX_VAL; float sy = MAX_VAL; float sz = MAX_VAL;
		float lx = MIN_VAL; float ly = MIN_VAL; float lz = MIN_VAL;
		for (uint n = 0; n < mesh->normalFaces.size(); ++n) {
			FaceBuf* buf = mesh->normalFaces[n];
			for (int i = 0; i < buf->count; ++i) {
				int index = mesh->indices[buf->start + i];

				vec4 vertex = mesh->vertices[index];
				vertex = scale(size.x, size.y, size.z) * vertex;
				float invw = 1.0 / vertex.w;
				vertex.x *= invw;
				vertex.y *= invw;
				vertex.z *= invw;
				
				sx = sx > vertex.x ? vertex.x : sx;
				sy = sy > vertex.y ? vertex.y : sy;
				sz = sz > vertex.z ? vertex.z : sz;
				lx = lx < vertex.x ? vertex.x : lx;
				ly = ly < vertex.y ? vertex.y : ly;
				lz = lz < vertex.z ? vertex.z : lz;
			}
		}
		vec3 halfSize = vec3(lx - sx, ly - sy, lz - sz) * 0.5;
		halfSize *= mesh->getBoundScale();
		collisionShape = new CollisionShape(halfSize);
	}
}

CollisionObject* Object::initCollisionObject() {
	if (!collisionObject)
		collisionObject = new CollisionObject(collisionShape->shape, mass);
	else {
		collisionObject->setCollisionShape(collisionShape->shape);
		collisionObject->setMass(mass);
	}
	collisionObject->object->setUserPointer(this);
	if (mass > 0) 
		collisionObject->object->setActivationState(DISABLE_DEACTIVATION);
	return collisionObject;
}

void Object::removeCollisionObject() {
	collisionObject = NULL;
}

void Object::updateLocalMatrices() {
	vertexTransform();
	normalTransform();
}

void Object::bindMaterial(int mid) {
	material = mid;
}

bool Object::checkInCamera(Camera* camera) {
	if (!bounding) return true;
	return bounding->checkWithCamera(camera->frustum, detailLevel);
}

bool Object::sphereInCamera(Camera* camera) {
	if (!bounding) return true;
	return bounding->sphereWithCamera(camera->frustum);
}

void Object::setBillboard(float sx, float sy, int mid) {
	if (billboard) delete billboard;
	billboard = new Billboard(sx, sy, mid);
}

void Object::updateObjectTransform(bool translate, bool rotate) {
	if (translate) {
		transformMatrix = parent->nodeTransform * localTransformMatrix;
		transformTransposed = transformMatrix.GetTranspose();
	}
	if (translate || rotate) {
		AABB* bbox = (AABB*)bounding;
		if (!bbox) bbox = (AABB*)parent->boundingBox;
		boundInfo = vec4(bbox->sizex, bbox->sizey, bbox->sizez, bbox->position.y);
	}
	if (transforms && translate) {
		vec3 transPos = GetTranslate(transformMatrix);
		transforms[0] = transPos.x;
		transforms[1] = transPos.y;
		transforms[2] = transPos.z;
		transforms[3] = size.x;
		updateSoundsPosition(transPos);
	}
	if (transformsFull) {
		if (translate) 
			memcpy(transformsFull, transforms, 4 * sizeof(buff));
		if (rotate) {
			transformsFull[4] = (rotateQuat.x);
			transformsFull[5] = (rotateQuat.y);
			transformsFull[6] = (rotateQuat.z);
			transformsFull[7] = (rotateQuat.w);
		}
		if (translate || rotate) {
			transformsFull[8] = (boundInfo.x);
			transformsFull[9] = (boundInfo.y);
			transformsFull[10] = (boundInfo.z);
			transformsFull[11] = (boundInfo.w);
		}
	}
}

void Object::updateSoundsPosition(const vec3& position) {
	std::map<std::string, SoundObject*>::iterator it;
	for (it = sounds.begin(); it != sounds.end(); ++it)
		it->second->setPosition(position);
}

void Object::setSound(const char* name, const char* path) {
	std::map<std::string, SoundObject*>::iterator it = sounds.find(name);
	if (it != sounds.end()) delete it->second;
	else sounds[name] = new SoundObject(path);
}

SoundObject* Object::getSound(const char* name) { 
	std::map<std::string, SoundObject*>::iterator it = sounds.find(name);
	return (it == sounds.end()) ? NULL : it->second;
}