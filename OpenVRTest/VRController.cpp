#include "VRController.h"
#include <glmSupport.h>
#include "VRTools.h"

using namespace renderlib;

string getRenderModelErrorString(vr::EVRRenderModelError error) {
	switch (error) {
	case vr::VRRenderModelError_BufferTooSmall:
		return "BufferTooSmall";
	case vr::VRRenderModelError_InvalidArg:
		return "InvalidArg";
	case vr::VRRenderModelError_InvalidModel:
		return "InvalidModel";
	case vr::VRRenderModelError_InvalidTexture:
		return "InvalidTexture";
	case vr::VRRenderModelError_Loading:
		return "Loading";
	case vr::VRRenderModelError_MultipleShapes:
		return "MultipleShapes";
	case vr::VRRenderModelError_MultipleTextures:
		return "MultipleTextures";
	case vr::VRRenderModelError_NoShapes:
		return "NoShapes";
	case vr::VRRenderModelError_NotEnoughNormals:
		return "NotEnoughNormals";
	case vr::VRRenderModelError_NotEnoughTexCoords:
		return "NotEnoughTexCoords";
	case vr::VRRenderModelError_NotSupported:
		return "NotSupported";
	case vr::VRRenderModelError_TooManyVertices:
		return "TooManyVertices";
	}
}

VRController::VRController() :renderModel(nullptr) {

}

VRController::VRController(vr::TrackedDeviceIndex_t index, vr::IVRSystem *vrSystem,
	vr::TrackedDevicePose_t pose, TextureManager *texManager) :
	index(index)
{
	updatePose(pose);

	char nameBuffer[1024];
	vrSystem->GetStringTrackedDeviceProperty(index,
		vr::Prop_RenderModelName_String, nameBuffer, 1024);

	vr::EVRRenderModelError error = vr::VRRenderModels()->LoadRenderModel_Async(
		nameBuffer, &renderModel);

	while (error == vr::VRRenderModelError_Loading)
		error = vr::VRRenderModels()->LoadRenderModel_Async(
			nameBuffer, &renderModel);

	if (error == vr::VRRenderModelError_None)
		openvrRenderModelToDrawable(this, renderModel, texManager);
	else {
		printf("RenderModelError - %s\n", getRenderModelErrorString(error).c_str());
	}
}

void VRController::updatePose(const vr::TrackedDevicePose_t &pose) {
	vr::HmdMatrix34_t poseMatrix = pose.mDeviceToAbsoluteTracking;
	position = getTranslation(poseMatrix);
	orientation = normalize(quat_cast(getRotation(poseMatrix)));
}

void VRController::updateState(const vr::VRControllerState_t &state) {
	//Mild hack to get index of trigger. Better approach would be to use https://github.com/ValveSoftware/openvr/issues/56
	int triggerIndex = vr::k_EButton_SteamVR_Trigger - vr::k_EButton_Axis0;
	int trackpadIndex = vr::k_EButton_SteamVR_Touchpad - vr::k_EButton_Axis0;
	axes[TRIGGER_AXIS] = vec2(state.rAxis[triggerIndex].x, 0.f);
	axes[TRACKPAD_AXIS] = vec2(state.rAxis[trackpadIndex].x, state.rAxis[trackpadIndex].y);
	buttons[TRACKPAD_BUTTON] = vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad) & state.ulButtonPressed;
	buttons[GRIP_BUTTON] = vr::ButtonMaskFromId(vr::k_EButton_Grip) & state.ulButtonPressed;
	buttons[TRIGGER_BUTTON] = vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger) & state.ulButtonPressed;
	buttons[TRACKPAD_BUTTON] = vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad) & state.ulButtonPressed;
	trackpadTouched = vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad) & state.ulButtonTouched;
}

void VRController::loadModelMatrixOldOpenGL() const {
	mat4 transform = getTransform();

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glMultMatrixf(&transform[0][0]);

}

VRSceneTransform::VRSceneTransform() :
	Object(vec3(0.f), quat()), scale(1.f),
	velocity(0.f), angularVelocity(), controllers(nullptr) {}


VRSceneTransform::VRSceneTransform(vector<VRController> *controllers) :
	Object(vec3(0.f), quat()), scale(1.f),
	velocity(0.f), angularVelocity(), controllers(controllers) {
	linkControllers(controllers);
}

void VRSceneTransform::linkControllers(vector<VRController> *newControllers) {
	controllers = newControllers;
	lastPosition.resize(controllers->size());
	lastOrientation.resize(controllers->size());

	for (int i = 0; i < controllers->size(); i++) {
		lastPosition[i] = controllers->at(i).getPos();
		lastOrientation[i] = controllers->at(i).getOrientationQuat();
	}
}

mat4 VRSceneTransform::getTransform() const {
	mat4 rigidTransform = Object::getTransform();
	mat4 scaleMatrix = mat4(
		scale, 0, 0, 0,
		0, scale, 0, 0,
		0, 0, scale, 0,
		0, 0, 0, 1);

	return rigidTransform*scaleMatrix;
}

void VRSceneTransform::multMatrixOldOpenGL() {
	/*	mat4 rigidTransform = getTransform();
	mat4 scaleMatrix = mat4(
	scale, 0, 0, 0,
	0, scale, 0, 0,
	0, 0, scale, 0,
	0, 0, 0, 1);
	*/
	mat4 transform = getTransform();	//rigidTransform*scaleMatrix;

	glMatrixMode(GL_MODELVIEW_MATRIX);
	glMultMatrixf(&transform[0][0]);
}

//Not currently incorporating time - FIX
void VRSceneTransform::updateTransform(float deltaTime) {

	//Get indices of controllers which have the grip pressed
	std::vector<int> gripsPressed;
	for (int i = 0; i < controllers->size(); i++) {
		if (controllers->at(i).buttons[VRController::GRIP_BUTTON])
			gripsPressed.push_back(i);
	}

	switch (gripsPressed.size()) {
	case 1:
	{
		int index = gripsPressed[0];
		velocity = controllers->at(index).getPos() - lastPosition[index];
		angularVelocity = quat();
		break;
	}
	case 2:
	{
		int indexA = gripsPressed[0];
		int indexB = gripsPressed[1];

		vec3 axisA = lastPosition[indexA] - lastPosition[indexB];
		vec3 axisB = controllers->at(indexA).getPos()
			- controllers->at(indexB).getPos();
		float lengthA = length(axisA);
		float lengthB = length(axisB);
		axisA = axisA / lengthA;
		axisB = axisB / lengthB;

		scale *= lengthB / lengthA;		//Rescale model

		vec3 rotAxis = cross(axisA, axisB);
		if (length(rotAxis) > 0.0001f) {
			float angle = asin(length(rotAxis));
			angularVelocity = angleAxis(angle, normalize(rotAxis));
		}
		velocity = vec3(0.f);
		break;
	}
	default:
	{
		velocity *= 0.99f;
		angularVelocity = slerp(angularVelocity, quat(), 0.01f);
	}
	}

	//Integrate velocities
	position += velocity;
	orientation = normalize(angularVelocity*orientation);

	//Save positions
	for (int i = 0; i < lastPosition.size(); i++) {
		lastPosition[i] = controllers->at(i).getPos();
		lastOrientation[i] = controllers->at(i).getOrientationQuat();
	}
}

bool VRSceneTransform::multMatrixPreviewTransform(float modelScale) {
	std::vector<int> gripsPressed;
	for (int i = 0; i < controllers->size(); i++) {
		if (controllers->at(i).buttons[VRController::GRIP_BUTTON])
			gripsPressed.push_back(i);
	}
	if (gripsPressed.size() < 2)
		return false;

	vec3 controllerPos[2];
	controllerPos[0] = controllers->at(gripsPressed[0]).getPos();
	controllerPos[1] = controllers->at(gripsPressed[1]).getPos();
	float controllerDist = length(controllerPos[0] - controllerPos[1]);
	float newScale = controllerDist / (2.f*modelScale);
	vec3 controllerMidpoint = (controllerPos[0] + controllerPos[1])*0.5f;

	mat4 translationToMidpoint = translateMatrix(controllerMidpoint);
	mat4 scaleBetweenControllers = scaleMatrix(newScale);

	mat4 modelMatrix = translationToMidpoint*scaleBetweenControllers*getOrientationMat4();

	glMatrixMode(GL_MODELVIEW);
	//	glPushMatrix();
	glMultMatrixf(&modelMatrix[0][0]);

	return true;
}

