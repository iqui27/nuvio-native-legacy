# Respostas rascunho: #401, #402, #403 (2026-10-09)

Nao postadas. Para o dono revisar. Sem promessa de data.

## #401 (Long update time on Homebrew channel)

Thanks for the report, and sorry about the wait. The package is about the same size as 2.0.2 (roughly 60 MB), so the size alone should not explain 15 minutes; the Homebrew Channel shows Downloading, Verifying and Installing as separate steps, so if you can tell us which one took the longest it would help a lot. We are also looking at making the package smaller in a coming release.

## #402 (Wrong sorting in sources)

Thanks, the screenshot made this clear. Your formatter writes the quality as "FHD" without the number 1080, and the app only reads the number, so those sources land in "Other"; we have found the spot in the source parser and will teach it to read "FHD". The one marked "N/A" really does not state a resolution, so it will stay in "Other".

## #403 (Add a next button to go to next episode)

Thanks for the suggestion. Today the next-episode card only shows up in the credits or the last seconds, and if it is dismissed the only way is the Episodes list in the player controls. A Next button in the controls is a good idea and we have added it to the roadmap.
