// Copyright Tempo Simulation, LLC. All Rights Reserved

//! Integration tests for the generated `tempo-sim` Rust client against a packaged sim.
//!
//! Scripts/TestRust.sh launches the packaged sim (headless) before running these and tears it down
//! after. They mirror the Python `core`/`world` groups. Run via Scripts/TestRust.sh integration.
//! Run single-threaded (the script passes --test-threads=1) since they share the client's global
//! connection context and drive the same sim.

use std::time::{Duration, Instant};

use tempo_sim::proto::tempo_core::{Transform, Vector};

/// Point the client at the sim and block until its gRPC server answers (the TCP port can be up
/// before gRPC is ready, so retry the actual call).
fn connect() {
    let port: u16 = std::env::var("TEMPO_SERVER_PORT")
        .ok()
        .and_then(|s| s.parse().ok())
        .unwrap_or(10001);
    tempo_sim::set_server("localhost", port);

    let deadline = Instant::now() + Duration::from_secs(120);
    loop {
        match tempo_sim::tempo_core::get_current_level_name() {
            Ok(_) => return,
            Err(e) => {
                if Instant::now() >= deadline {
                    panic!("sim gRPC server not ready: {e}");
                }
                std::thread::sleep(Duration::from_millis(500));
            }
        }
    }
}

#[test]
fn level_name_is_reported() {
    connect();
    let resp = tempo_sim::tempo_core::get_current_level_name().expect("get_current_level_name");
    assert!(!resp.level.is_empty(), "expected a non-empty current level name");
}

#[test]
fn spawn_query_destroy_round_trip() {
    connect();

    // StaticMeshActor is an engine class (always available) and spawns with no mesh/collision, so
    // the requested location is preserved.
    let transform = Transform {
        location: Some(Vector { x: 1.0, y: 2.0, z: 3.0 }),
        ..Default::default()
    };
    let spawned = tempo_sim::tempo_world::spawn_actor("StaticMeshActor".into(), false, transform, "".into())
        .expect("spawn_actor");
    assert!(!spawned.name.is_empty(), "spawn_actor returned an empty name");

    let state = tempo_sim::tempo_world::get_current_actor_state(spawned.name.clone(), false)
        .expect("get_current_actor_state");
    let loc = state
        .transform
        .expect("actor state has a transform")
        .location
        .expect("transform has a location");
    assert!((loc.x - 1.0).abs() < 0.05, "x was {}", loc.x);
    assert!((loc.y - 2.0).abs() < 0.05, "y was {} (handedness round trip)", loc.y);
    assert!((loc.z - 3.0).abs() < 0.05, "z was {}", loc.z);

    tempo_sim::tempo_world::destroy_actor(spawned.name).expect("destroy_actor");
}

#[test]
fn actor_names_omit_blueprint_suffix_and_resolve() {
    connect();

    // Unreal names the class a Blueprint generates "BP_Foo_C", and that suffix leaks into instance
    // names ("BP_Foo_C_1"). Tempo reports the class name as-is but strips the suffix from instance
    // names, and every name it reports must resolve on the way back in. See the naming banner in
    // TempoWorld/WorldControl.proto.
    let actors = tempo_sim::tempo_world::get_all_actors()
        .expect("get_all_actors")
        .actors;

    for actor in &actors {
        // A Blueprint instance's object name is its class name plus an index, so if the suffix
        // survived, the name would still start with the full actor_type.
        if actor.actor_type.ends_with("_C") {
            assert!(
                !actor.name.starts_with(&actor.actor_type),
                "actor name '{}' kept the _C suffix from '{}'",
                actor.name,
                actor.actor_type
            );
        }

        // The reported name resolves: get_all_components' only failure mode is an unknown actor.
        tempo_sim::tempo_world::get_all_components(actor.name.clone())
            .unwrap_or_else(|e| panic!("get_all_components('{}'): {e}", actor.name));
    }
}
