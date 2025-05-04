import React, { useState, useCallback, useEffect } from 'react';
import { useDropzone } from 'react-dropzone';
import styled from '@emotion/styled';
import { io } from 'socket.io-client';

const Container = styled.div`
  padding: 40px 0 0 120px;
  font-family: 'Inter', sans-serif;
  background: #fff;
  min-height: 100vh;
`;

const Title = styled.h1`
  font-size: 2.5rem;
  margin-bottom: 40px;
`;

const MainContent = styled.div`
  display: flex;
  gap: 60px;
`;

const UploadSection = styled.div`
  border: 2px dashed #bdbdbd;
  border-radius: 24px;
  width: 500px;
  height: 450px;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  position: relative;
  cursor: pointer;
  transition: border-color 0.2s ease;

  &:hover {
    border-color: #4f7cff;
  }
`;

const PreviewImage = styled.img`
  max-width: 80%;
  max-height: 250px;
  object-fit: contain;
  margin: 20px 0;
`;

const Text = styled.p`
  font-size: 1.3rem;
  margin: 0 0 10px 0;
  text-align: center;
`;

const BrowseButton = styled.button`
  margin: 10px 0;
  padding: 8px 24px;
  border-radius: 8px;
  border: 1px solid #222;
  background: #fff;
  cursor: pointer;
  transition: all 0.2s ease;

  &:hover {
    background: #f5f5f5;
  }
`;

const UploadIcon = styled.div`
  font-size: 3rem;
  margin: 20px 0;
`;

const FormatText = styled.p`
  color: #bdbdbd;
  font-size: 0.9rem;
  position: absolute;
  bottom: 60px;
`;

const AnalyzeButton = styled.button<{ disabled: boolean }>`
  background: ${props => props.disabled ? '#bdbdbd' : '#4f7cff'};
  color: #fff;
  border: none;
  position: absolute;
  bottom: 20px;
  right: 30px;
  padding: 8px 24px;
  border-radius: 8px;
  cursor: ${props => props.disabled ? 'not-allowed' : 'pointer'};
  transition: background 0.2s ease;

  &:hover {
    background: ${props => props.disabled ? '#bdbdbd' : '#3d63cc'};
  }
`;

const ResultSection = styled.div`
  border: 1px solid #4f7cff;
  border-radius: 24px;
  width: 400px;
  padding: 32px 36px;
  min-height: 350px;
  background: #fff;
`;

const ResultItem = styled.div`
  margin-bottom: 24px;
`;

const ResultLabel = styled.div`
  font-weight: 600;
  margin-bottom: 8px;
`;

const ResultValue = styled.div`
  color: #333;
  line-height: 1.5;
`;

const SocketStatus = styled.div<{ connected: boolean }>`
  position: fixed;
  top: 20px;
  right: 20px;
  padding: 8px 16px;
  border-radius: 20px;
  background: ${props => props.connected ? '#4CAF50' : '#f44336'};
  color: white;
  font-size: 14px;
  display: flex;
  align-items: center;
  gap: 8px;

  &::before {
    content: '';
    width: 8px;
    height: 8px;
    border-radius: 50%;
    background: ${props => props.connected ? '#fff' : '#fff'};
    display: inline-block;
  }
`;

const RealtimeSection = styled.div`
  margin-top: 40px;
  padding: 20px;
  border: 1px solid #4f7cff;
  border-radius: 24px;
  width: 100%;
  max-width: 1000px;
`;

const RealtimeTitle = styled.h2`
  font-size: 1.5rem;
  margin-bottom: 20px;
  color: #333;
`;

const RealtimeContent = styled.div`
  display: flex;
  gap: 40px;
  flex-wrap: wrap;
`;

const RealtimeImage = styled.img`
  max-width: 300px;
  height: auto;
  border-radius: 12px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
`;

const RealtimeResults = styled.div`
  flex: 1;
  min-width: 300px;
`;

const TabContainer = styled.div`
  margin-top: 40px;
  width: 100%;
  max-width: 1200px;
`;

const TabButtons = styled.div`
  display: flex;
  gap: 20px;
  margin-bottom: 20px;
`;

const TabButton = styled.button<{ active: boolean }>`
  padding: 10px 20px;
  border: none;
  border-radius: 8px;
  background: ${props => props.active ? '#4f7cff' : '#f5f5f5'};
  color: ${props => props.active ? '#fff' : '#333'};
  cursor: pointer;
  font-size: 16px;
  transition: all 0.2s ease;

  &:hover {
    background: ${props => props.active ? '#4f7cff' : '#e5e5e5'};
  }
`;

const HistoryContainer = styled.div`
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(300px, 1fr));
  gap: 20px;
  padding: 20px 0;
`;

const HistoryCard = styled.div`
  border: 1px solid #e0e0e0;
  border-radius: 12px;
  overflow: hidden;
  transition: transform 0.2s ease, box-shadow 0.2s ease;
  background: white;

  &:hover {
    transform: translateY(-5px);
    box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
  }
`;

const CardImage = styled.img`
  width: 100%;
  height: 200px;
  object-fit: cover;
  border-bottom: 1px solid #e0e0e0;
`;

const CardContent = styled.div`
  padding: 16px;
`;

const CardTitle = styled.h3`
  margin: 0 0 12px 0;
  color: #333;
  font-size: 18px;
`;

const CardDetail = styled.div`
  margin-bottom: 8px;
  font-size: 14px;

  strong {
    color: #666;
    margin-right: 6px;
  }
`;

interface AnalysisResponse {
  decision: string;
  disease_name: string;
  reason: string;
  treatment: string;
  base64_image?: string; // For REST API response
}

interface SocketResponse extends AnalysisResponse {
    base64_image: string; // For Socket.IO response
}



const LeafDiseaseDetection: React.FC = () => {
  const [files, setFiles] = useState<File[]>([]);
  const [preview, setPreview] = useState<string>('');
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [analysisResult, setAnalysisResult] = useState<AnalysisResponse | null>(null);
  const [error, setError] = useState<string>('');
  const [socketConnected, setSocketConnected] = useState(false);
  const [realtimeData, setRealtimeData] = useState<SocketResponse | null>(null);
  const [activeTab, setActiveTab] = useState<'realtime' | 'history'>('realtime');
  const [historicalData, setHistoricalData] = useState<AnalysisResponse[]>([]);

  // Initialize Socket.IO connection
  useEffect(() => {
    const socket = io('http://127.0.0.1:5000');

    socket.on('connect', () => {
      setSocketConnected(true);
      console.log('Socket connected');
    });

    socket.on('disconnect', () => {
      setSocketConnected(false);
      console.log('Socket disconnected');
    });

    socket.on('result', (data: SocketResponse) => {
      console.log('Received realtime analysis:', data);
      setRealtimeData(data);
    });

    return () => {
      socket.disconnect();
    };
  }, []);

  useEffect(() => {
    // Fetch historical data
    const fetchHistoricalData = async () => {
      try {
        const response = await fetch('http://127.0.0.1:5000/history');
        if (!response.ok) {
          throw new Error('Failed to fetch historical data');
        }
        const data = await response.json();
        setHistoricalData(data);
      } catch (error) {
        console.error('Error fetching historical data:', error);
      }
    };

    fetchHistoricalData();
  }, [isAnalyzing,realtimeData]);

  const onDrop = useCallback((acceptedFiles: File[]) => {
    setFiles(acceptedFiles);
    setAnalysisResult(null);
    setError('');
    
    if (acceptedFiles.length > 0) {
      const file = acceptedFiles[0];
      const previewUrl = URL.createObjectURL(file);
      setPreview(previewUrl);
    }
  }, []);

  const { getRootProps, getInputProps } = useDropzone({
    accept: {
      'image/*': ['.png', '.jpg', '.jpeg']
    },
    onDrop,
    multiple: false
  });

  const handleAnalyze = async (e: React.MouseEvent) => {
    e.stopPropagation();
    if (files.length === 0) return;

    setIsAnalyzing(true);
    setError('');

    const formData = new FormData();
    formData.append('image', files[0]);

    try {
      const response = await fetch('http://127.0.0.1:5000/analyze', {
        method: 'POST',
        body: formData,
      });

      if (!response.ok) {
        throw new Error('Analysis failed');
      }

      const result = await response.json();
      setAnalysisResult(result);
      // If there's a base64_image in the response, use it for preview
      if (result.base64_image) {
        setPreview(`data:image/jpeg;base64,${result.base64_image}`);
      }
    } catch (err) {
      setError('Failed to analyze image. Please try again.');
      console.error('Analysis error:', err);
    } finally {
      setIsAnalyzing(false);
    }
  };

  const renderHistoricalData = () => {
    if (historicalData.length === 0) {
      return (
        <ResultValue style={{ color: '#666', textAlign: 'center' }}>
          No historical data available
        </ResultValue>
      );
    }

    return (
      <HistoryContainer>
        {historicalData.map((item, index) => (
          <HistoryCard key={index}>
            <CardImage 
              src={`data:image/jpeg;base64,${item.base64_image}`}
              alt={`Historical leaf ${index + 1}`}
            />
            <CardContent>
              <CardTitle>{item.disease_name}</CardTitle>
              <CardDetail>
                <strong>Decision:</strong> {item.decision}
              </CardDetail>
              <CardDetail>
                <strong>Reason:</strong> {item.reason}
              </CardDetail>
              <CardDetail>
                <strong>Treatment:</strong> {item.treatment}
              </CardDetail>
            </CardContent>
          </HistoryCard>
        ))}
      </HistoryContainer>
    );
  };

  return (
    <Container>
      <SocketStatus connected={socketConnected}>
        {socketConnected ? 'Connected' : 'Disconnected'}
      </SocketStatus>
      
      <Title>Plant Leaf Disease Detection</Title>
      <MainContent>
        <UploadSection {...getRootProps()}>
          <input {...getInputProps()} />
          {preview ? (
            <PreviewImage src={preview} alt="Leaf preview" />
          ) : (
            <>
              <Text>
                Drag files to upload
                <br />
                Or
              </Text>
              <BrowseButton type="button">Browse Files</BrowseButton>
              <UploadIcon>↑</UploadIcon>
            </>
          )}
          <FormatText>Formats accepted are .png, .jpg and .jpeg</FormatText>
          <AnalyzeButton 
            onClick={handleAnalyze}
            disabled={files.length === 0 || isAnalyzing}
          >
            {isAnalyzing ? 'Analyzing...' : 'Analyze'}
          </AnalyzeButton>
        </UploadSection>
        <ResultSection>
          {error && (
            <ResultItem>
              <ResultValue style={{ color: 'red' }}>{error}</ResultValue>
            </ResultItem>
          )}
          {analysisResult ? (
            <>
              <ResultItem>
                <ResultLabel>Disease Name:</ResultLabel>
                <ResultValue>{analysisResult.disease_name}</ResultValue>
              </ResultItem>
              <ResultItem>
                <ResultLabel>Decision:</ResultLabel>
                <ResultValue>{analysisResult.decision}</ResultValue>
              </ResultItem>
              <ResultItem>
                <ResultLabel>Reason:</ResultLabel>
                <ResultValue>{analysisResult.reason}</ResultValue>
              </ResultItem>
              <ResultItem>
                <ResultLabel>Treatment:</ResultLabel>
                <ResultValue>{analysisResult.treatment}</ResultValue>
              </ResultItem>
            </>
          ) : (
            <ResultValue style={{ color: '#666', textAlign: 'center', marginTop: '40px' }}>
              Upload an image and click Analyze to see the results
            </ResultValue>
          )}
        </ResultSection>
      </MainContent>

      <TabContainer>
        <TabButtons>
          <TabButton 
            active={activeTab === 'realtime'} 
            onClick={() => setActiveTab('realtime')}
          >
            Real-time Feed
          </TabButton>
          <TabButton 
            active={activeTab === 'history'} 
            onClick={() => setActiveTab('history')}
          >
            Historical Data
          </TabButton>
        </TabButtons>

        {activeTab === 'realtime' ? (
          <RealtimeSection>
            <RealtimeTitle>Real-time Analysis Feed</RealtimeTitle>
            <RealtimeContent>
              {realtimeData ? (
                <>
                  <RealtimeImage 
                    src={`data:image/jpeg;base64,${realtimeData.base64_image}`} 
                    alt="Real-time analyzed leaf" 
                  />
                  <RealtimeResults>
                    <ResultItem>
                      <ResultLabel>Disease Name:</ResultLabel>
                      <ResultValue>{realtimeData.disease_name}</ResultValue>
                    </ResultItem>
                    <ResultItem>
                      <ResultLabel>Decision:</ResultLabel>
                      <ResultValue>{realtimeData.decision}</ResultValue>
                    </ResultItem>
                    <ResultItem>
                      <ResultLabel>Reason:</ResultLabel>
                      <ResultValue>{realtimeData.reason}</ResultValue>
                    </ResultItem>
                    <ResultItem>
                      <ResultLabel>Treatment:</ResultLabel>
                      <ResultValue>{realtimeData.treatment}</ResultValue>
                    </ResultItem>
                  </RealtimeResults>
                </>
              ) : (
                <ResultValue style={{ color: '#666' }}>
                  Waiting for real-time analysis data...
                </ResultValue>
              )}
            </RealtimeContent>
          </RealtimeSection>
        ) : (
          renderHistoricalData()
        )}
      </TabContainer>
    </Container>
  );
};

export default LeafDiseaseDetection; 