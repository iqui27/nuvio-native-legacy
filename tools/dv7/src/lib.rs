//! Small, stateless C bridge to libdovi. No video decoding or frame buffering.
use dolby_vision::rpu::dovi_rpu::DoviRpu;

/// Convert one escaped HEVC UNSPEC62 NAL (including its two-byte header).
/// Returns its output size, or -1 on malformed/unsupported metadata or capacity.
///
/// # Safety
/// Input and output must reference non-overlapping buffers of the given sizes.
#[no_mangle]
pub unsafe extern "C" fn nv_dv7_rpu(
    input: *const u8, len: usize, output: *mut u8, capacity: usize,
) -> isize {
    if input.is_null() || output.is_null() || !(3..=8192).contains(&len) {
        return -1;
    }
    let result = (|| {
        let mut rpu = DoviRpu::parse_unspec62_nalu(std::slice::from_raw_parts(input, len)).ok()?;
        // Never reinterpret profile 5, 8.4 or unrelated data as HDR10-compatible.
        if rpu.dovi_profile != 7 && rpu.dovi_profile != 8 { return None; }
        rpu.convert_with_mode(2).ok()?;
        rpu.write_hevc_unspec62_nalu().ok()
    })();
    match result {
        Some(bytes) if bytes.len() <= capacity => {
            std::ptr::copy_nonoverlapping(bytes.as_ptr(), output, bytes.len());
            bytes.len() as isize
        }
        _ => -1,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    const FEL: &[u8] = include_bytes!("../../../tests/fixtures/dv7/fel_orig.bin");
    const MEL: &[u8] = include_bytes!("../../../tests/fixtures/dv7/mel_orig.bin");

    #[test]
    fn mel_and_fel_become_single_layer_profile_81() {
        for fixture in [MEL, FEL] {
            let original = DoviRpu::parse_unspec62_nalu(fixture).unwrap();
            assert_eq!(original.dovi_profile, 7);
            let input = original.write_hevc_unspec62_nalu().unwrap();
            let mut output = [0; 8192];
            let size = unsafe { nv_dv7_rpu(input.as_ptr(), input.len(), output.as_mut_ptr(), output.len()) };
            assert!(size > 0);
            let converted = DoviRpu::parse_unspec62_nalu(&output[..size as usize]).unwrap();
            assert_eq!(converted.dovi_profile, 8);
            assert!(converted.el_type.is_none());
            assert!(converted.header.disable_residual_flag);
            assert_eq!(format!("{:?}",converted.vdr_dm_data), format!("{:?}",original.vdr_dm_data));
            // Export valid escaped NALs for the C packet and engine fixtures.
            if let Ok(dir) = std::env::var("NV_DV7_FIXTURE_OUT") {
                let name = if fixture.len() == MEL.len() { "mel" } else { "fel" };
                std::fs::write(format!("{dir}/{name}-input.nal"), &input).unwrap();
                std::fs::write(format!("{dir}/{name}-output.nal"), &output[..size as usize]).unwrap();
            }
        }
    }

    #[test]
    fn rejects_corrupt_rpu_and_small_output() {
        let valid = DoviRpu::parse_unspec62_nalu(FEL).unwrap().write_hevc_unspec62_nalu().unwrap();
        let mut output = [0; 8192];
        assert_eq!(unsafe { nv_dv7_rpu(valid.as_ptr(), valid.len(), output.as_mut_ptr(), 1) }, -1);
        let bad = [0x7c, 1, 0x19, 0, 0, 0, 0, 0];
        assert_eq!(unsafe { nv_dv7_rpu(bad.as_ptr(), bad.len(), output.as_mut_ptr(), output.len()) }, -1);
        assert_eq!(unsafe { nv_dv7_rpu(std::ptr::null(), 20, output.as_mut_ptr(), output.len()) }, -1);
    }
}
