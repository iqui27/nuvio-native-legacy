// Request telemetry only. A successful API return does not prove that the
// firmware retained a ROI: explicit recovery requests must still reach it.
namespace NuvioTpk
{
    internal sealed class VideoWindowMetrics
    {
        object owner, pendingOwner;
        int x, y, w, h, pendingX, pendingY, pendingW, pendingH;
        bool valid, full, pendingFull;
        internal long Requests { get; private set; }
        internal long RepeatedRequests { get; private set; }
        internal long Applied { get; private set; }
        internal long Failed { get; private set; }
        internal long ElapsedTicks { get; private set; }
        internal long MaxTicks { get; private set; }

        // Called on the session thread, immediately before native work.
        internal void Begin(object player, int rx, int ry, int rw, int rh, bool fullscreen)
        {
            Requests++;
            if (valid && object.ReferenceEquals(owner, player) && x == rx && y == ry &&
                w == rw && h == rh && full == fullscreen) RepeatedRequests++;
            pendingOwner = player; pendingX = rx; pendingY = ry;
            pendingW = rw; pendingH = rh; pendingFull = fullscreen;
            // A failed Mode/SetRoi must not remain a successful baseline.
            valid = false;
        }

        internal void Complete(bool success, long elapsedTicks)
        {
            if (elapsedTicks < 0) elapsedTicks = 0;
            ElapsedTicks += elapsedTicks;
            if (elapsedTicks > MaxTicks) MaxTicks = elapsedTicks;
            if (!success) { Failed++; return; }
            Applied++;
            owner = pendingOwner; x = pendingX; y = pendingY;
            w = pendingW; h = pendingH; full = pendingFull; valid = true;
        }

        internal void Invalidate() { valid = false; }
        internal void Reset()
        {
            owner = pendingOwner = null; valid = false;
            Requests = RepeatedRequests = Applied = Failed = ElapsedTicks = MaxTicks = 0;
        }
    }
}
