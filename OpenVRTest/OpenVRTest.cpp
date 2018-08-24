// OpenVRTest.cpp : Defines the entry point for the console application.
//

#include "VRWindow.h"
#include <string>
#include <iostream>

#include "ConvexHull.h"

int main(int argc, char** argv)
{
	SlotMap<int> map;

	SlotMap<int>::Index a = map.add(1);
	SlotMap<int>::Index b = map.add(2);
	SlotMap<int>::Index c = map.add(3);
	map.remove(b);
	SlotMap<int>::Index d = map.add(4);
	SlotMap<int>::Index e = map.add(5);

	for (auto it = map.begin(); it != map.end(); ++it) {
		std::cout << (*it) << std::endl;
	}
	
	HalfEdgeMesh<glm::vec3> mesh;


	WindowManager wm(800, 400, "VR Segmenting");
	char* loadFilename = "untrackedmodels/riccoSurface_take3.obj";	//"models/dragon.obj";
	char* saveFilename = "saved/ricco.clr";
	int multisampling = 8;
	switch (argc) {
	case 1:
		//Change to ricco
		loadFilename = "untrackedmodels/riccoSurface_take3.obj";
		saveFilename = "saved/dragon.clr";
		break;
	case 2:
		loadFilename = argv[1];
		saveFilename = argv[1];
		break;
	case 3:
		loadFilename = argv[1];
		saveFilename = argv[2];
		break;
	case 4:
		loadFilename = argv[1];
		saveFilename = argv[2];
		multisampling = std::stoi(argv[3]);
	}

	wm.paintingLoop(loadFilename, saveFilename, multisampling);
}