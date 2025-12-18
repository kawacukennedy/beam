// Example usage of the error handling system

use crate::{Core, Error, Result as CoreResult};

pub fn example_usage() -> CoreResult<()> {
    // Create core instance
    let core = Core::new("test_key", None)?;

    // Add a device - this will return a typed error if it fails
    let device = bluebeam_database::Device {
        id: "device1".to_string(),
        name: "Test Device".to_string(),
        address: "00:11:22:33:44:55".to_string(),
        trusted: false,
        last_seen: chrono::Utc::now(),
        fingerprint: "fingerprint".to_string(),
    };

    core.add_device(&device)?;

    // If an error occurs, it will be properly typed and can be handled
    // match result {
    //     Ok(_) => println!("Success!"),
    //     Err(e) => {
    //         println!("Error code: {}", e.error_code() as u32);
    //         println!("User message: {}", e.user_message());
    //         println!("Detailed error: {}", e);
    //     }
    // }

    Ok(())
}