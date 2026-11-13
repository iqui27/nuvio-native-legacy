//! Small, stateless C bridge to libdovi. No video decoding or frame buffering.
use dolby_vision::rpu::dovi_rpu::DoviRpu;
use dolby_vision::rpu::ConversionMode;

/// Validate an existing P8 RPU without converting or modifying its metadata.
/// # Safety
/// Input must reference `len` readable bytes.
#[no_mangle]
pub unsafe extern "C" fn nv_dv8_rpu_valid(input: *const u8, len: usize) -> i32 {
    if input.is_null() || !(3..=8192).contains(&len) { return 0; }
    match DoviRpu::parse_unspec62_nalu(std::slice::from_raw_parts(input, len)) {
        Ok(rpu) if rpu.dovi_profile == 8 => 1,
        _ => 0,
    }
}

/// Convert one escaped HEVC UNSPEC62 NAL (including its two-byte header).
/// Returns its output size, or -1 on malformed/unsupported metadata or capacity.
///
/// # Safety
/// Input and output must reference non-overlapping buffers of the given sizes.
#[no_mangle]
pub unsafe extern "C" fn nv_dv7_rpu(
    input: *const u8, len: usize, output: *mut u8, capacity: usize,
) -> isize {
    nv_dv7_rpu_mapping(input,len,output,capacity,0)
}

/// Experimental per-session P7 conversion. Never use numeric mode 4: the
/// library numeric conversion maps that to P8.4, not mapping preservation.
/// # Safety
/// Same non-overlapping input/output contract as `nv_dv7_rpu`.
#[no_mangle]
pub unsafe extern "C" fn nv_dv7_rpu_mapping(
    input: *const u8,len: usize,output: *mut u8,capacity: usize,preserve: i32,
) -> isize {
    nv_dv7_rpu_mapping_report(input,len,output,capacity,preserve,std::ptr::null_mut())
}

/// Same conversion with a first-RPU diagnostic witness. Bits: P7 to P8 (1),
/// mapping retained (2), residual disabled (4), bytes differ from standard (8).
/// # Safety
/// Same byte-buffer contract; non-null report must reference a writable u32.
#[no_mangle]
pub unsafe extern "C" fn nv_dv7_rpu_mapping_report(
    input: *const u8,len: usize,output: *mut u8,capacity: usize,preserve: i32,report: *mut u32,
) -> isize {
    if !report.is_null() { *report=0; }
    if !matches!(preserve,0 | 1) || input.is_null() || output.is_null() || !(3..=8192).contains(&len) {
        return -1;
    }
    let result = (|| {
        let mut rpu = DoviRpu::parse_unspec62_nalu(std::slice::from_raw_parts(input, len)).ok()?;
        // Never reinterpret profile 5, 8.4 or unrelated data as HDR10-compatible.
        if rpu.dovi_profile != 7 && rpu.dovi_profile != 8 { return None; }
        let profile=rpu.dovi_profile;
        let witness=if !report.is_null() {
            let curves=format!("{:?}",rpu.rpu_data_mapping.as_ref().map(|m|&m.curves));
            let mut standard=rpu.clone();standard.convert_with_mode(ConversionMode::To81).ok()?;
            Some((curves,standard.write_hevc_unspec62_nalu().ok()?))
        } else {None};
        let mode=if preserve==1 && rpu.dovi_profile==7 {
            ConversionMode::To81MappingPreserved
        } else { ConversionMode::To81 };
        rpu.convert_with_mode(mode).ok()?;
        let bytes=rpu.write_hevc_unspec62_nalu().ok()?;
        let mut flags=0;
        if let Some((curves,standard))=witness {
            if profile==7 && rpu.dovi_profile==8 {flags|=1;}
            if curves==format!("{:?}",rpu.rpu_data_mapping.as_ref().map(|m|&m.curves)) {flags|=2;}
            if rpu.header.disable_residual_flag {flags|=4;}
            if bytes!=standard {flags|=8;}
        }
        Some((bytes,flags))
    })();
    match result {
        Some((bytes,flags)) if bytes.len() <= capacity => {
            std::ptr::copy_nonoverlapping(bytes.as_ptr(), output, bytes.len());
            if !report.is_null() {*report=flags;}
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
    fn p8_validation_leaves_metadata_unchanged() {
        let mut rpu = DoviRpu::parse_unspec62_nalu(MEL).unwrap();
        let p7 = rpu.write_hevc_unspec62_nalu().unwrap();
        rpu.convert_with_mode(2).unwrap();
        let p8 = rpu.write_hevc_unspec62_nalu().unwrap();
        let before = p8.clone();
        assert_eq!(unsafe { nv_dv8_rpu_valid(p8.as_ptr(), p8.len()) }, 1);
        assert_eq!(p8, before);
        assert_eq!(unsafe { nv_dv8_rpu_valid(p7.as_ptr(), p7.len()) }, 0);
        assert_eq!(unsafe { nv_dv8_rpu_valid(std::ptr::null(), 20) }, 0);
        assert_eq!(unsafe { nv_dv8_rpu_valid(p8.as_ptr(), 8193) }, 0);
        let bad = [0x7c, 1, 0x19, 0, 0, 0];
        assert_eq!(unsafe { nv_dv8_rpu_valid(bad.as_ptr(), bad.len()) }, 0);
    }

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
    fn mapping_opt_in_preserves_all_three_curves_without_restoring_el() {
        for (name,fixture) in [("mel",MEL),("fel",FEL)] {
            let original=DoviRpu::parse_unspec62_nalu(fixture).unwrap();
            let input=original.write_hevc_unspec62_nalu().unwrap();
            let curves=format!("{:?}",original.rpu_data_mapping.as_ref().unwrap().curves);
            let mut off=vec![0;8192];let mut on=vec![0;8192];
            let a=unsafe {nv_dv7_rpu(input.as_ptr(),input.len(),off.as_mut_ptr(),off.len())};
            let b=unsafe {nv_dv7_rpu_mapping(input.as_ptr(),input.len(),on.as_mut_ptr(),on.len(),1)};
            assert!(a>0 && b>0);off.truncate(a as usize);on.truncate(b as usize);
            let mut proof=[0;8192];let mut report=0;
            let n=unsafe {nv_dv7_rpu_mapping_report(input.as_ptr(),input.len(),proof.as_mut_ptr(),8192,1,&mut report)};
            assert_eq!(n,b);assert_eq!(&proof[..n as usize],on.as_slice());
            assert_eq!(report&7,7);if name=="fel" {assert_ne!(report&8,0);}
            report=99;
            assert_eq!(unsafe {nv_dv7_rpu_mapping_report(input.as_ptr(),input.len(),proof.as_mut_ptr(),1,1,&mut report)},-1);
            assert_eq!(report,0);
            let mut reference=original.clone();reference.convert_with_mode(2).unwrap();
            assert_eq!(off,reference.write_hevc_unspec62_nalu().unwrap());
            let result=DoviRpu::parse_unspec62_nalu(&on).unwrap();
            assert_eq!(result.dovi_profile,8);
            assert!(result.header.disable_residual_flag && result.el_type.is_none());
            let mapping=result.rpu_data_mapping.as_ref().unwrap();
            assert!(mapping.nlq.is_none());
            assert_eq!(format!("{:?}",mapping.curves),curves);
            assert_eq!(format!("{:?}",result.vdr_dm_data),format!("{:?}",reference.vdr_dm_data));
            if name=="fel" {assert_ne!(off,on);}
            let mut invalid=[0;8192];
            assert_eq!(unsafe {nv_dv7_rpu_mapping(input.as_ptr(),input.len(),invalid.as_mut_ptr(),1,1)},-1);
            assert_eq!(unsafe {nv_dv7_rpu_mapping(input.as_ptr(),input.len(),invalid.as_mut_ptr(),8192,4)},-1);
            assert_eq!(unsafe {nv_dv7_rpu_mapping(std::ptr::null(),input.len(),invalid.as_mut_ptr(),8192,1)},-1);
            let corrupt=[0x7c,1,0x19,0,0,0];
            assert_eq!(unsafe {nv_dv7_rpu_mapping(corrupt.as_ptr(),corrupt.len(),invalid.as_mut_ptr(),8192,1)},-1);
            // The P7 experiment cannot reinterpret existing P8 metadata.
            let n=unsafe {nv_dv7_rpu_mapping(on.as_ptr(),on.len(),invalid.as_mut_ptr(),8192,1)};
            assert!(n>0);assert_eq!(&invalid[..n as usize],on.as_slice());
            if let Ok(dir)=std::env::var("NV_DV7_FIXTURE_OUT") {
                std::fs::write(format!("{dir}/{name}-mapping-output.nal"),on).unwrap();
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
