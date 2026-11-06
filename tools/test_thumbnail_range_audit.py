import tempfile
import unittest
from pathlib import Path
from thumbnail_range_audit import analyse, merge, intersection, size


class RangeAuditTest(unittest.TestCase):
    def fixture(self, folder, unknown=False, truncated=False):
        lines = ["# version=1", "# duration_ms=65000", "# source_bytes=100000",
                 "# truncated=" + str(truncated).lower(),
                 "kind,index,pts_ns,start,end_exclusive,selected"]
        # Current policy takes exact target; smaller following indexed frame
        # remains within the SAME 5s tolerance. All fixtures are metadata only.
        for k in range(1, 7):
            start = k * 1000
            lines.append(f"frame,{k*2},{k*10000*1000000},{start},{start+100},1")
            a, b = (-1, -1) if unknown and k == 3 else (start+200, start+250)
            lines.append(f"frame,{k*2+1},{(k*10000+1000)*1000000},{a},{b},0")
            lines.append(f"download,{k},0,{start},{start+100},0")
        lines.append("download,99,0,1000,1100,0")
        file = Path(folder) / "audit.csv"
        file.write_text("\n".join(lines))
        return file

    def test_union_counts_overlapping_and_adjacent_spans_once(self):
        self.assertEqual(merge([(10, 20), (15, 25), (25, 30), (40, 50)]), [(10, 30), (40, 50)])
        self.assertEqual(size([(10, 20), (15, 25)]), 15)
        self.assertEqual(intersection([(10, 25)], [(0, 12), (20, 30)]), 7)

    def test_smaller_candidate_savings_preserve_target_set(self):
        with tempfile.TemporaryDirectory() as d:
            result = analyse(self.fixture(d))
            audit, sim = result["byteAudit"], result["simulation"]
            self.assertEqual(audit["successfulParsedRangeBytes"], 700)
            self.assertEqual(audit["uniqueDownloadedBytes"], 600)
            self.assertEqual(audit["repeatedDownloadedBytes"], 100)
            self.assertEqual(sim["baselineUniqueBlockBytes"], 600)
            self.assertEqual(sim["smallestUniqueBlockBytes"], 300)
            self.assertEqual(sim["savingPercent"], 50)
            self.assertTrue(sim["allTargetSlotsComparable"])

    def test_unknown_sizes_are_excluded_not_invented(self):
        with tempfile.TemporaryDirectory() as d:
            sim = analyse(self.fixture(d, unknown=True))["simulation"]
            self.assertEqual(sim["comparableSlots"], 5)
            self.assertEqual(sim["slotsExcludedForUnknownCandidateSizes"], 1)
            self.assertFalse(sim["allTargetSlotsComparable"])

    def test_truncation_and_relaxed_tolerance_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaises(ValueError):
                analyse(self.fixture(d, truncated=True))
            with self.assertRaises(ValueError):
                analyse(self.fixture(d), tolerance_ms=8000)

    def test_aliases_do_not_count_same_frame_twice(self):
        self.assertEqual(size([(100, 200), (100, 200)]), 100)

    def test_sample_is_not_presented_as_full_movie_coverage(self):
        with tempfile.TemporaryDirectory() as d:
            file=self.fixture(d)
            file.write_text("# sample_targets_ms=10000,60000\n# probe_status=complete\n"+file.read_text())
            result=analyse(file)
            self.assertEqual(result["scope"], "sampled-header-probe-simulation")
            self.assertEqual(result["simulation"]["evaluationTargetSlots"], 2)
            self.assertEqual(result["simulation"]["comparableSlots"], 2)
            self.assertTrue(result["simulation"]["allSampledSlotsComparable"])
            self.assertFalse(result["simulation"]["allTargetSlotsComparable"])


if __name__ == "__main__":
    unittest.main()
