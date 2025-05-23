using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Input;
using System.Windows.Threading;

namespace GroupChatClient
{
    public partial class MainWindow : Window
    {
        private readonly ObservableCollection<Message> _messages;
        private bool _isConnected; private readonly DispatcherTimer _timer;
        private int _messageIndex; private readonly string[] _users = { "User1", "User2" };
    
        public MainWindow()
        {
            InitializeComponent();
            _messages = new ObservableCollection<Message>();
            DataContext = new { Messages = _messages };
            _isConnected = false;
            _messageIndex = 0;

            // Initialize timer for updating message timestamps
            _timer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(10) };
            _timer.Tick += Timer_Tick;
            _timer.Start();
        }

        // File Menu: Connect
        private void ConnectMenuItem_Click(object sender, RoutedEventArgs e)
        {
            if (!_isConnected)
            {
                _isConnected = true;
                ConnectMenuItem.IsEnabled = false;
                DisconnectMenuItem.IsEnabled = true;
                AddSystemMessage("Connected");
            }
        }

        // File Menu: Disconnect
        private void DisconnectMenuItem_Click(object sender, RoutedEventArgs e)
        {
            if (_isConnected)
            {
                _isConnected = false;
                ConnectMenuItem.IsEnabled = true;
                DisconnectMenuItem.IsEnabled = false;
                AddSystemMessage("Disconnected");
            }
        }

        // File Menu: Exit
        private void ExitMenuItem_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }

        // Help Menu: About
        private void AboutMenuItem_Click(object sender, RoutedEventArgs e)
        {
            MessageBox.Show(
                "Group Chat Client\nCreated for WPF Lab\nVersion 1.0",
                "About",
                MessageBoxButton.OK,
                MessageBoxImage.Information
            );
        }

        // Send button click handler
        private void SendButton_Click(object sender, RoutedEventArgs e)
        {
            SendMessage();
        }

        // Handle Enter and Shift+Enter in the message input
        private void MessageTextBox_PreviewKeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter && !Keyboard.IsKeyDown(Key.LeftShift) && !Keyboard.IsKeyDown(Key.RightShift))
            {
                e.Handled = true;
                SendMessage();
            }
        }

        // Adjust TextBox height dynamically
        private void MessageTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (sender is TextBox textBox)
            {
                textBox.InvalidateMeasure();
            }
        }

        // Send a user message
        private void SendMessage()
        {
            if (string.IsNullOrWhiteSpace(MessageTextBox.Text))
            {
                return;
            }

            string username = _users[_messageIndex % 2];
            _messages.Add(new Message(MessageTextBox.Text, username, _messageIndex));
            MessageTextBox.Clear();
            _messageIndex++;
        }

        // Add a system message
        private void AddSystemMessage(string content)
        {
            _messages.Add(new Message(content, isSystemMessage: true));
        }

        // Update timestamps for all non-system messages
        private void Timer_Tick(object sender, EventArgs e)
        {
            foreach (Message message in _messages)
            {
                if (!message.IsSystemMessage)
                {
                    message.UpdateTimestamp();
                }
            }
        }
    }

public class Message : INotifyPropertyChanged
{
    private readonly DateTime _sentTime;
    private string _timestampText;

    public string Content { get; }
    public string Username { get; }
    public bool IsSystemMessage { get; }
    public bool IsEvenIndex { get; }
    public string TimestampText
    {
        get => _timestampText;
        private set
        {
            _timestampText = value;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(TimestampText)));
        }
    }
    public string FullTimestamp => _sentTime.ToString("dd/MM/yyyy HH:mm:ss");

    public Message(string content, bool isSystemMessage = false)
    {
        Content = content;
        IsSystemMessage = isSystemMessage;
        _sentTime = DateTime.Now;
        UpdateTimestamp();
    }

    public Message(string content, string username, int index)
    {
        Content = content;
        Username = username;
        IsSystemMessage = false;
        IsEvenIndex = index % 2 == 0;
        _sentTime = DateTime.Now;
        UpdateTimestamp();
    }

    public void UpdateTimestamp()
    {
        TimeSpan timeDiff = DateTime.Now - _sentTime;
        if (timeDiff.TotalMinutes < 1)
        {
            TimestampText = "Now";
        }
        else if (timeDiff.TotalMinutes < 15)
        {
            TimestampText = $"{(int)timeDiff.TotalMinutes}m ago";
        }
        else if (timeDiff.TotalDays < 1)
        {
            TimestampText = _sentTime.ToString("HH:mm");
        }
        else
        {
            TimestampText = _sentTime.ToString("dd/MM/yyyy");
        }
    }

    public event PropertyChangedEventHandler PropertyChanged;
}

public class BooleanToAlignmentConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
    {
        return (bool)value ? new Thickness(40,10,10,10) : new Thickness(10,10,40,10);
    }

    public object ConvertBack(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
    {
        throw new NotImplementedException();
    }
}

    public class MessageTemplateSelector : DataTemplateSelector
    {
        public override DataTemplate SelectTemplate(object item, DependencyObject container)
        {
            if (item is not Message message || container is not FrameworkElement element)
            {
                return null;
            }

            return message.IsSystemMessage
                ? element.FindResource("SystemMessageTemplate") as DataTemplate
                : element.FindResource("UserMessageTemplate") as DataTemplate;
        }
    }
}