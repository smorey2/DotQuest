#pragma once
#ifndef VR_CLIENT_INFO_H
#define VR_CLIENT_INFO_H

typedef struct {
	bool initialized;
	int width;
	int height;
	float fov;
	float screen_dist;
	float playerHeight;
	float playerYaw;

} vr_client_info_t;

#endif