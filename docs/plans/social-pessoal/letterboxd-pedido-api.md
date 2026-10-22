> RASCUNHO para o dono. Nao foi enviado a ninguem: revise, preencha o contato e mande voce mesmo.
> O texto abaixo esta em ingles porque e o que o Letterboxd le. Nada nele promete prazo nem volume que nao existam.
> O plano (secao 6.5) segue valendo sem a API: importar/exportar por CSV, so link de review, sem RSS.

# Request for Letterboxd API access: Nuvio Legacy

Hello Letterboxd team,

I am the developer of **Nuvio Legacy**, a free, non-commercial media app for older smart TVs (LG webOS and Samsung Tizen) that keeps a private per-profile watch diary. I would like to ask whether you grant API access to small independent apps like ours, and what the process would be.

## What we would use it for

1. **Read the member's own data, with their OAuth consent.** Their diary entries, ratings and the URL of their own reviews, to fill the diary inside the app when they choose to connect their account.
2. **Write on the member's behalf, only when they ask.** Create a diary entry or a rating for a film they just finished watching, from a button in the app ("Log on Letterboxd"). Nothing is posted automatically.

Today the only thing we do is let members export a CSV in your import format and import their own export ZIP, entirely in their browser. The API would replace that manual step for those who prefer it.

## What we will not do

- No scraping of letterboxd.com, and no automated reading of pages we are not authorised to read.
- No monitoring of RSS feeds or any polling of other members' activity.
- No data resale, no advertising use, no building a competing catalogue or social graph from your data.
- No access to other members' private data; we only act on the account of the member who connected it.
- We would follow your branding and attribution rules, and your rate limits.

## Privacy

- Tokens are stored only for the member who connected, can be revoked at any time (inside the app and on your side), and deleting the diary or disconnecting removes them from our server.
- We store only what the feature needs (film, date, rating, tags, and the link to the member's review). We do not copy review text.
- Data is never shared with third parties and never shown to other users.

## Expected volume

A few thousand members at most, each syncing on demand (a handful of requests per session, not continuous polling).

## Contact

- Name: [YOUR NAME]
- App: Nuvio Legacy, [PROJECT URL]
- Email: [CONTACT EMAIL]

Thank you for your time, and for the importer that makes all of this possible without any API.

[YOUR NAME]
