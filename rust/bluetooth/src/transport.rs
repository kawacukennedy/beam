use crate::Result;
use tokio::sync::mpsc;

#[async_trait::async_trait]
pub trait Transport: Send + Sync {
    async fn send_packet(&self, data: &[u8]) -> Result<()>;
    async fn receive_packets(&self) -> Result<mpsc::Receiver<Vec<u8>>>;
    async fn close(&self) -> Result<()>;

    async fn send(&self, data: &[u8]) -> Result<()> {
        self.send_packet(data).await
    }

    async fn receive(&self) -> Result<Vec<u8>> {
        let mut rx = self.receive_packets().await?;
        rx.recv().await.ok_or_else(|| crate::BluetoothError("Channel closed".to_string()))
    }
}