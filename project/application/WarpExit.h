#pragma once
#define NOMINMAX

#include <memory>
#include <string>
#include "math/Vector3.h"
#include "math/Vector4.h"

class Object3d;
class Object3dCommon;
class Camera;

/// @brief Warp destination actor.
/// Spawned dynamically when the player touches a warp entrance.
/// Automatically expires after a set lifetime or when the player
/// has entered and then left its trigger volume.
class WarpExit {
public:
	WarpExit() = default;
	~WarpExit() = default;

	/// @brief Initialize the exit actor at the specified position.
	/// @param objCommon  Object3dCommon for rendering setup
	/// @param camera     Active camera
	/// @param position   World position of the exit
	/// @param modelName  Model name to display (e.g. "Cube.obj")
	void Initialize(Object3dCommon* objCommon, Camera* camera,
	                const Vector3& position, const std::string& modelName = "Cube.obj");

	/// @brief Update per frame. Counts down lifetime and tracks
	///        whether the player has entered and then left the trigger volume.
	/// @param playerPosition  Current player world position
	void Update(const Vector3& playerPosition);

	/// @brief Draw the exit actor.
	void Draw();

	/// @brief Returns true when this exit should be destroyed.
	///        Conditions: lifetime expired OR player entered then left.
	bool IsExpired() const;

	/// @brief Get the world position of this exit actor.
	Vector3 GetPosition() const { return position_; }

private:
	// --- Transform ---
	Vector3 position_ = {};

	// --- Trigger ---
	// Sphere-based overlap detection radius
	float triggerRadius_ = 5.0f;

	// --- Lifetime ---
	int lifetimeFrames_    = 180;  // Remaining frames until auto-destroy (default 3 sec @ 60fps)
	int maxLifetimeFrames_ = 180;

	// --- Player overlap state ---
	bool hasPlayerEntered_    = false;  // Player has been inside at least once
	bool playerCurrentlyInside_ = false; // Player is inside this frame
	bool playerLeftAfterEnter_ = false;  // Player entered then left -> ready to destroy

	// --- Rendering ---
	std::unique_ptr<Object3d> object_ = nullptr;

	// --- Visual ---
	Vector4 exitColor_ = { 0.2f, 0.9f, 0.8f, 0.5f }; // Semi-transparent cyan
	Vector3 exitScale_ = { 1.5f, 1.5f, 1.5f };
};
