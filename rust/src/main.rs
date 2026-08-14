use eframe::egui;
use chrono::Local;
use std::fs;
use std::io::{Read, Write};
use std::net::TcpStream;
use std::path::PathBuf;
use std::sync::mpsc::{channel, Receiver, Sender};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::thread;
use std::time::{Duration, Instant};

struct StreamFrame {
    jpeg_bytes: Vec<u8>,
    color_image: egui::ColorImage,
}

#[derive(PartialEq)]
enum ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Error(String),
}

struct Esp32CamApp {
    ip_address: String,
    stream_path: String,
    state: ConnectionState,
    stop_signal: Arc<AtomicBool>,
    frame_rx: Option<Receiver<StreamFrame>>,
    current_texture: Option<egui::TextureHandle>,
    last_raw_jpeg: Option<Vec<u8>>,
    status_msg: String,
    notification: Option<(String, Instant)>,
    fps: f32,
    frame_count: usize,
    last_frame_time: Instant,
    flash_state: bool,
}

impl Default for Esp32CamApp {
    fn default() -> Self {
        Self {
            ip_address: "127.0.0.1:8080".to_string(),
            stream_path: "/stream".to_string(),
            state: ConnectionState::Disconnected,
            stop_signal: Arc::new(AtomicBool::new(false)),
            frame_rx: None,
            current_texture: None,
            last_raw_jpeg: None,
            status_msg: "Ready. Enter ESP32 IP/mDNS address (e.g. esp32-cam.local:81)".to_string(),
            notification: None,
            fps: 0.0,
            frame_count: 0,
            last_frame_time: Instant::now(),
            flash_state: false,
        }
    }
}

impl Esp32CamApp {
    fn connect(&mut self, ctx: egui::Context) {
        self.disconnect();
        self.state = ConnectionState::Connecting;
        self.status_msg = format!("Connecting to {}...", self.ip_address);
        
        let stop_signal = Arc::new(AtomicBool::new(false));
        self.stop_signal = stop_signal.clone();

        let (tx, rx) = channel::<StreamFrame>();
        self.frame_rx = Some(rx);

        let ip_target = self.ip_address.clone();
        let path_target = self.stream_path.clone();

        thread::spawn(move || {
            stream_worker(ip_target, path_target, stop_signal, tx, ctx);
        });
    }

    fn disconnect(&mut self) {
        self.stop_signal.store(true, Ordering::SeqCst);
        self.state = ConnectionState::Disconnected;
        self.frame_rx = None;
        self.current_texture = None;
        self.last_raw_jpeg = None;
        self.status_msg = "Disconnected.".to_string();
    }

    fn toggle_flash_led(&mut self) {
        let (host, _) = parse_host_port(&self.ip_address);
        let addr = format!("{}:80", host);
        let flash_target = host.clone();

        self.flash_state = !self.flash_state;
        let new_state = self.flash_state;

        thread::spawn(move || {
            if let Ok(mut stream) = TcpStream::connect_timeout(
                &addr.parse().unwrap_or_else(|_| "127.0.0.1:8080".parse().unwrap()),
                Duration::from_secs(2),
            ) {
                let req = format!("GET /flash HTTP/1.1\r\nHost: {}\r\nConnection: close\r\n\r\n", flash_target);
                let _ = stream.write_all(req.as_bytes());
            }
        });

        self.notification = Some((
            format!("⚡ Flash LED command sent ({})", if new_state { "ON" } else { "OFF" }),
            Instant::now(),
        ));
    }

    fn capture_snapshot(&mut self) {
        let raw_jpeg = match &self.last_raw_jpeg {
            Some(data) => data.clone(),
            None => {
                self.notification = Some((
                    "⚠️ No video frame available to capture!".to_string(),
                    Instant::now(),
                ));
                return;
            }
        };

        let pictures_dir = PathBuf::from("/home/computer/pictures");
        if let Err(e) = fs::create_dir_all(&pictures_dir) {
            self.notification = Some((
                format!("❌ Failed to create pictures dir: {}", e),
                Instant::now(),
            ));
            return;
        }

        let now = Local::now();
        let filename = format!("ESP32_CAM_{}.jpg", now.format("%Y%m%d_%H%M%S_%3f"));
        let target_file = pictures_dir.join(&filename);

        match fs::write(&target_file, &raw_jpeg) {
            Ok(_) => {
                let msg = format!("📸 Snapshot saved to {}", target_file.display());
                println!("[+] {}", msg);
                self.notification = Some((msg, Instant::now()));
            }
            Err(e) => {
                self.notification = Some((
                    format!("❌ Error saving image: {}", e),
                    Instant::now(),
                ));
            }
        }
    }
}

fn parse_host_port(input: &str) -> (String, u16) {
    let clean = input.trim().trim_start_matches("http://").trim_start_matches("https://");
    let host_port = clean.split('/').next().unwrap_or(clean);
    
    if let Some((host, port_str)) = host_port.split_once(':') {
        let port = port_str.parse::<u16>().unwrap_or(80);
        (host.to_string(), port)
    } else {
        (host_port.to_string(), 80)
    }
}

fn stream_worker(
    target: String,
    path: String,
    stop: Arc<AtomicBool>,
    tx: Sender<StreamFrame>,
    ctx: egui::Context,
) {
    let (host, port) = parse_host_port(&target);
    let addr = format!("{}:{}", host, port);

    let mut stream = match TcpStream::connect_timeout(
        &addr.parse().unwrap_or_else(|_| "127.0.0.1:8080".parse().unwrap()),
        Duration::from_secs(5),
    ) {
        Ok(s) => s,
        Err(e) => {
            eprintln!("Connection failed: {}", e);
            return;
        }
    };

    let request = format!(
        "GET {} HTTP/1.1\r\nHost: {}\r\nUser-Agent: ESP32CamStreamer/1.0\r\nConnection: keep-alive\r\n\r\n",
        path, host
    );

    if stream.write_all(request.as_bytes()).is_err() {
        return;
    }
    let _ = stream.set_read_timeout(Some(Duration::from_secs(3)));

    let mut buffer = Vec::with_capacity(65536);
    let mut chunk = [0u8; 8192];

    while !stop.load(Ordering::Relaxed) {
        match stream.read(&mut chunk) {
            Ok(0) => break,
            Ok(n) => {
                buffer.extend_from_slice(&chunk[..n]);

                // Scan for JPEG delimiters 0xFF 0xD8 (SOI) to 0xFF 0xD9 (EOI)
                while let Some(soi) = find_subslice(&buffer, &[0xFF, 0xD8]) {
                    if let Some(relative_eoi) = find_subslice(&buffer[soi..], &[0xFF, 0xD9]) {
                        let eoi = soi + relative_eoi + 2;
                        let jpeg_bytes = buffer[soi..eoi].to_vec();

                        // Remove processed data from buffer
                        buffer.drain(0..eoi);

                        // Decode image
                        if let Ok(dyn_img) = image::load_from_memory_with_format(&jpeg_bytes, image::ImageFormat::Jpeg) {
                            let rgb = dyn_img.to_rgb8();
                            let size = [rgb.width() as usize, rgb.height() as usize];
                            let pixels = rgb.into_raw();
                            let color_image = egui::ColorImage::from_rgb(size, &pixels);

                            let frame = StreamFrame {
                                jpeg_bytes,
                                color_image,
                            };

                            if tx.send(frame).is_err() {
                                return;
                            }
                            ctx.request_repaint();
                        }
                    } else {
                        break;
                    }

                    if buffer.len() > 1_000_000 {
                        buffer.clear();
                    }
                }
            }
            Err(_) => {
                thread::sleep(Duration::from_millis(10));
            }
        }
    }
}

fn find_subslice(haystack: &[u8], needle: &[u8]) -> Option<usize> {
    haystack.windows(needle.len()).position(|window| window == needle)
}

impl eframe::App for Esp32CamApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        // Receive frames from background thread
        if let Some(ref rx) = self.frame_rx {
            while let Ok(frame) = rx.try_recv() {
                self.last_raw_jpeg = Some(frame.jpeg_bytes);
                self.current_texture = Some(ctx.load_texture(
                    "esp32-stream-frame",
                    frame.color_image,
                    egui::TextureOptions::LINEAR,
                ));
                self.state = ConnectionState::Connected;
                self.frame_count += 1;
                
                let now = Instant::now();
                let elapsed = now.duration_since(self.last_frame_time).as_secs_f32();
                if elapsed > 0.5 {
                    self.fps = 1.0 / (elapsed / self.frame_count.max(1) as f32);
                    self.last_frame_time = now;
                    self.frame_count = 0;
                }
            }
        }

        // Global Key Listener for Ctrl + X
        ctx.input(|i| {
            if i.modifiers.ctrl && i.key_pressed(egui::Key::X) {
                self.capture_snapshot();
            }
        });

        // UI Panel Setup
        egui::TopBottomPanel::top("header_panel").show(ctx, |ui| {
            ui.add_space(6.0);
            ui.horizontal(|ui| {
                ui.heading("📷 ESP32-CAM Streamer & Capture");
                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                    if ui.button("📸 Capture (Ctrl+X)").clicked() {
                        self.capture_snapshot();
                    }
                    ui.add_space(8.0);
                    if ui.button("⚡ Flash LED").clicked() {
                        self.toggle_flash_led();
                    }
                });
            });
            ui.add_space(6.0);
        });

        egui::TopBottomPanel::top("control_panel").show(ctx, |ui| {
            ui.horizontal(|ui| {
                ui.label("IP / Host:");
                ui.add(egui::TextEdit::singleline(&mut self.ip_address).desired_width(170.0));

                ui.label("Path:");
                ui.add(egui::TextEdit::singleline(&mut self.stream_path).desired_width(80.0));

                if self.state == ConnectionState::Connected || self.state == ConnectionState::Connecting {
                    if ui.button("⏹ Disconnect").clicked() {
                        self.disconnect();
                    }
                } else {
                    if ui.button("▶ Connect").clicked() {
                        self.connect(ctx.clone());
                    }
                }

                ui.separator();
                ui.label("Presets:");
                if ui.button("mDNS (esp32-cam.local)").clicked() {
                    self.ip_address = "esp32-cam.local:81".to_string();
                    self.stream_path = "/stream".to_string();
                    self.connect(ctx.clone());
                }
                if ui.button("Local Sim (127.0.0.1:8080)").clicked() {
                    self.ip_address = "127.0.0.1:8080".to_string();
                    self.stream_path = "/stream".to_string();
                    self.connect(ctx.clone());
                }
            });
            ui.add_space(4.0);
        });

        egui::TopBottomPanel::bottom("status_bar").show(ctx, |ui| {
            ui.horizontal(|ui| {
                let status_str = match &self.state {
                    ConnectionState::Disconnected => "🔴 Disconnected",
                    ConnectionState::Connecting => "🟡 Connecting...",
                    ConnectionState::Connected => "🟢 Streaming Live",
                    ConnectionState::Error(e) => e.as_str(),
                };
                ui.label(status_str);
                ui.separator();
                ui.label(format!("FPS: {:.1}", self.fps));
                ui.separator();

                if let Some((ref msg, created)) = self.notification {
                    if created.elapsed().as_secs() < 5 {
                        ui.colored_label(egui::Color32::YELLOW, msg);
                    }
                } else {
                    ui.label(&self.status_msg);
                }
            });
        });

        egui::CentralPanel::default().show(ctx, |ui| {
            if let Some(ref texture) = self.current_texture {
                let available_size = ui.available_size();
                ui.centered_and_justified(|ui| {
                    ui.image((texture.id(), available_size));
                });
            } else {
                ui.centered_and_justified(|ui| {
                    ui.vertical_centered(|ui| {
                        ui.add_space(80.0);
                        ui.label(egui::RichText::new("📹 No Stream Active").size(22.0).strong());
                        ui.add_space(10.0);
                        ui.label("Enter ESP32 IP/mDNS (esp32-cam.local:81) and click Connect.");
                        ui.add_space(15.0);
                        ui.label(egui::RichText::new("Shortcuts: Ctrl + X (Snapshot)  |  ⚡ Flash LED button (Toggle Light)").italics());
                    });
                });
            }
        });
    }
}

fn main() -> eframe::Result<()> {
    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_title("ESP32-CAM Streamer & Capture (mDNS / OTA / Flash)")
            .with_inner_size([840.0, 620.0]),
        ..Default::default()
    };

    eframe::run_native(
        "ESP32-CAM Streamer",
        options,
        Box::new(|_cc| Box::new(Esp32CamApp::default())),
    )
}
