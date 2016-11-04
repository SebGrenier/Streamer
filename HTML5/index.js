var fs = require("fs"),
    http = require("http"),
    url = require("url"),
    path = require("path");

http.createServer(function (req, res) {

  console.log('Request', req.url);

  if (!req.url.startsWith("/live/video.mp4")) {
    res.writeHead(200, { "Content-Type": "text/html" });
    res.end('<video src="http://localhost:8888/live/video.mp4?t=' + new Date() + '" width="640" controls autoplay></video>');
  } else {
    var file = path.resolve(__dirname,"live/video.mp4");
    fs.stat(file, function(err, stats) {
      if (err) {
        if (err.code === 'ENOENT') {
          // 404 Error if file not found
          res.writeHead(404);
          return res.end();
        }
      res.end(err);
      }
      var range = req.headers.range;
      if (!range) {
       // 416 Wrong range
        res.writeHead(416);
        return res.end();
      }

      console.info('Range', range);

      var positions = range.replace(/bytes=/, "").split("-");
      var start = parseInt(positions[0], 10);
      var total = stats.size;
      const max_chunk_size = 32 * 1024;
      var end = Math.min(start + max_chunk_size, positions[1] ? parseInt(positions[1], 10) : total - 1);
      var chunksize = ((end - start) + 1);

      console.info('Streaming', 'chunk size', chunksize / 1024 / 1024, 'from', start / 1024 / 1024, 'to', end / 1024 / 1024);

      res.writeHead(206, {
        "Content-Range": "bytes " + start + "-" + end + "/" + total,
        "Accept-Ranges": "bytes",
        "Content-Length": chunksize,
        "Content-Type": "video/mp4",
        "Cache-Control": "no-store, no-cache, must-revalidate, max-age=0"
      });

      var stream = fs.createReadStream(file, { start: start, end: end })
        .on("open", function() {
          stream.pipe(res);
        }).on("error", function(err) {
			console.error(err);
          res.end(err);
        });
    });
  }
}).listen(8888);

console.log('Server started');
